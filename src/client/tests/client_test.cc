//  Copyright (c) 2014, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
#define __STDC_FORMAT_MACROS

#include <atomic>
#include <chrono>
#include <memory>
#include <numeric>
#include <mutex>
#include <thread>

#include "include/RocketSpeed.h"
#include "src/messages/msg_loop.h"
#include "src/messages/messages.h"
#include "src/port/Env.h"
#include "src/util/testharness.h"
#include "src/util/testutil.h"

namespace rocketspeed {

class MockConfiguration : public Configuration {
 public:
  Status GetPilot(HostId* host_out) const override {
    std::lock_guard<std::mutex> lock(mutex_);
    *host_out = pilot_;
    return !pilot_ ? Status::NotFound("") : Status::OK();
  }

  Status GetCopilot(HostId* host_out) const override {
    std::lock_guard<std::mutex> lock(mutex_);
    *host_out = copilot_;
    return !copilot_ ? Status::NotFound("") : Status::OK();
  }

  uint64_t GetCopilotVersion() const override { return version_.load(); }

  void SetPilot(HostId host) {
    std::lock_guard<std::mutex> lock(mutex_);
    pilot_ = host;
    ++version_;
  }

  void SetCopilot(HostId host) {
    std::lock_guard<std::mutex> lock(mutex_);
    copilot_ = host;
    ++version_;
  }

 private:
  mutable std::mutex mutex_;
  mutable std::atomic<uint64_t> version_;
  HostId pilot_;
  HostId copilot_;
};

class ClientTest {
 public:
  ClientTest()
  : positive_timeout(1000)
  , negative_timeout(100)
  , env_(Env::Default())
  , config_(std::make_shared<MockConfiguration>())
  , next_copilot_port_(5450) {
    ASSERT_OK(test::CreateLogger(env_, "ClientTest", &info_log_));
  }

  virtual ~ClientTest() { env_->WaitForJoin(); }

 protected:
  typedef std::chrono::steady_clock TestClock;
  // TODO(stupaq) generalise on next usage
  typedef std::atomic<MsgLoop*> CopilotAtomicPtr;

  const std::chrono::milliseconds positive_timeout;
  const std::chrono::milliseconds negative_timeout;
  Env* const env_;
  const std::shared_ptr<MockConfiguration> config_;
  std::shared_ptr<rocketspeed::Logger> info_log_;
  std::atomic<int> next_copilot_port_;

  class CopilotMock {
   public:
    // Noncopyable, movable
    CopilotMock(CopilotMock&&) = default;
    CopilotMock& operator=(CopilotMock&&) = default;

    CopilotMock(std::unique_ptr<MsgLoop> _msg_loop, std::thread msg_loop_thread)
    : msg_loop(std::move(_msg_loop))
    , msg_loop_thread_(std::move(msg_loop_thread)) {}

    ~CopilotMock() {
      msg_loop->Stop();
      if (msg_loop_thread_.joinable()) {
        msg_loop_thread_.join();
      }
    }

    std::unique_ptr<MsgLoop> msg_loop;

   private:
    std::thread msg_loop_thread_;
  };

  CopilotMock MockCopilot(
      const std::map<MessageType, MsgCallbackType>& callbacks) {
    std::unique_ptr<MsgLoop> copilot(new MsgLoop(
        env_, EnvOptions(), next_copilot_port_++, 1, info_log_, "copilot"));
    copilot->RegisterCallbacks(callbacks);
    ASSERT_OK(copilot->Initialize());
    std::thread thread([&]() { copilot->Run(); });
    ASSERT_OK(copilot->WaitUntilRunning());
    // Set copilot address in the configuration.
    config_->SetCopilot(copilot->GetHostId());
    return CopilotMock(std::move(copilot), std::move(thread));
  }

  std::unique_ptr<Client> CreateClient(ClientOptions options) {
    // Set very short tick.
    options.timer_period = std::chrono::milliseconds(1);
    // Override logger and configuration.
    options.info_log = info_log_;
    ASSERT_TRUE(options.config == nullptr);
    options.config = config_;
    std::unique_ptr<Client> client;
    ASSERT_OK(Client::Create(std::move(options), &client));
    return std::move(client);
  }
};

TEST(ClientTest, UnsubscribeDedup) {
  std::atomic<StreamID> last_origin;
  std::atomic<SubscriptionID> last_sub_id;
  port::Semaphore subscribe_sem, unsubscribe_sem;
  auto copilot = MockCopilot({
      {MessageType::mSubscribe,
       [&](std::unique_ptr<Message> msg, StreamID origin) {
         auto subscribe = static_cast<MessageDeliver*>(msg.get());
         last_origin = origin;
         last_sub_id = subscribe->GetSubID();
         subscribe_sem.Post();
       }},
      {MessageType::mUnsubscribe,
       [&](std::unique_ptr<Message> msg, StreamID origin) {
         auto unsubscribe = static_cast<MessageUnsubscribe*>(msg.get());
         ASSERT_EQ(last_sub_id.load() + 1, unsubscribe->GetSubID());
         unsubscribe_sem.Post();
       }},
  });

  auto dedup_timeout = 2 * negative_timeout;

  ClientOptions options;
  options.unsubscribe_deduplication_timeout = dedup_timeout;
  auto client = CreateClient(std::move(options));

  // Subscribe, so that we get stream ID of the client.
  client->Subscribe(GuestTenant, GuestNamespace, "UnsubscribeDedup", 0);
  ASSERT_TRUE(subscribe_sem.TimedWait(positive_timeout));

  MessageDeliverGap deliver(
      GuestTenant, last_sub_id.load() + 1, GapType::kBenign);

  // Send messages on a non-existent subscription.
  for (size_t i = 0; i < 10; ++i) {
    deliver.SetSequenceNumbers(i, i + 1);
    ASSERT_OK(copilot.msg_loop->SendResponse(deliver, last_origin.load(), 0));
  }
  // Should receive only one unsubscribe message.
  ASSERT_TRUE(unsubscribe_sem.TimedWait(positive_timeout));
  ASSERT_TRUE(!unsubscribe_sem.TimedWait(negative_timeout));

  // Wait for dedup period.
  ASSERT_TRUE(!unsubscribe_sem.TimedWait(dedup_timeout));

  // Publish another bad message.
  deliver.SetSequenceNumbers(11, 12);
  ASSERT_OK(copilot.msg_loop->SendResponse(deliver, last_origin.load(), 0));
  // Should receive unsubscribe message.
  ASSERT_TRUE(unsubscribe_sem.TimedWait(positive_timeout));
}

TEST(ClientTest, BackOff) {
  const size_t num_attempts = 4;
  std::vector<TestClock::duration> subscribe_attempts;
  port::Semaphore subscribe_sem;
  CopilotAtomicPtr copilot_ptr;
  auto copilot = MockCopilot(
      {{MessageType::mSubscribe,
        [&](std::unique_ptr<Message> msg, StreamID origin) {
          if (subscribe_attempts.size() >= num_attempts) {
            subscribe_sem.Post();
            return;
          }
          subscribe_attempts.push_back(TestClock::now().time_since_epoch());
          // Send back goodbye, so that client will resubscribe.
          MessageGoodbye goodbye(GuestTenant,
                                 MessageGoodbye::Code::Graceful,
                                 MessageGoodbye::OriginType::Server);
          // This is a tad fishy, but Copilot should not receive any message
          // before we perform the assignment to copilot_ptr.
          copilot_ptr.load()->SendResponse(goodbye, origin, 0);
        }}});
  copilot_ptr = copilot.msg_loop.get();

  // Back-off parameters.
  const uint64_t initial = 50 * 1000;
  const double base = 2.0;

  ClientOptions options;
  options.timer_period = std::chrono::milliseconds(1);
  options.backoff_initial = std::chrono::milliseconds(initial / 1000);
  options.backoff_base = base;
  options.backoff_distribution = [](ClientRNG*) { return 1.0; };
  auto client = CreateClient(std::move(options));

  // Subscribe and wait until enough reconnection attempts takes place.
  client->Subscribe(GuestTenant, GuestNamespace, "BackOff", 0);
  std::chrono::microseconds timeout(
      static_cast<uint64_t>(initial * std::pow(base, num_attempts)));
  ASSERT_TRUE(subscribe_sem.TimedWait(timeout));

  // Verify timeouts between consecutive attempts.
  ASSERT_EQ(num_attempts, subscribe_attempts.size());
  std::vector<TestClock::duration> differences(num_attempts);
  std::adjacent_difference(subscribe_attempts.begin(),
                           subscribe_attempts.end(),
                           differences.begin());
  for (size_t i = 1; i < num_attempts; ++i) {
    std::chrono::microseconds expected(
        static_cast<uint64_t>(initial * std::pow(base, i - 1)));
    ASSERT_TRUE(differences[i] >= expected - expected / 4);
    ASSERT_TRUE(differences[i] <= expected + expected / 4);
  }
}

TEST(ClientTest, GetCopilotFailure) {
  port::Semaphore subscribe_sem;
  auto copilot =
      MockCopilot({{MessageType::mSubscribe,
                    [&](std::unique_ptr<Message> msg, StreamID origin) {
                      subscribe_sem.Post();
                    }}});

  ClientOptions options;
  // Speed up client retries.
  options.timer_period = std::chrono::milliseconds(1);
  options.backoff_distribution = [](ClientRNG*) { return 0.0; };
  auto client = CreateClient(std::move(options));

  // Clear configuration entry for the Copilot.
  config_->SetCopilot(HostId());

  // Subscribe, no call should make it to the Copilot.
  auto sub_handle =
      client->Subscribe(GuestTenant, GuestNamespace, "GetCopilotFailure", 0);
  ASSERT_TRUE(!subscribe_sem.TimedWait(negative_timeout));

  // While disconnected, unsubscribe and subscribe again, this shouldn't affect
  // the scenario.
  client->Unsubscribe(sub_handle);
  client->Subscribe(GuestTenant, GuestNamespace, "GetCopilotFailure", 0);

  // Copilot shouldn't receive anything.
  ASSERT_TRUE(!subscribe_sem.TimedWait(negative_timeout));

  // Set Copilot address in the config.
  config_->SetCopilot(copilot.msg_loop->GetHostId());

  // Copilot should receive the subscribe request.
  ASSERT_TRUE(subscribe_sem.TimedWait(positive_timeout));
}

TEST(ClientTest, OfflineOperations) {
  std::unordered_map<Topic, std::pair<SequenceNumber, SubscriptionHandle>>
      subscriptions;
  port::Semaphore unsubscribe_sem, all_ok_sem;
  std::atomic<bool> expects_request(false);
  auto subscribe_cb = [&](std::unique_ptr<Message> msg, StreamID origin) {
    ASSERT_TRUE(expects_request.load());
    auto subscribe = static_cast<MessageSubscribe*>(msg.get());
    auto it = subscriptions.find(subscribe->GetTopicName());
    ASSERT_TRUE(it != subscriptions.end());
    ASSERT_EQ(it->second.first, subscribe->GetStartSequenceNumber());
    subscriptions.erase(it);
    if (subscriptions.empty()) {
      all_ok_sem.Post();
    }
  };
  auto copilot = MockCopilot({{MessageType::mSubscribe, subscribe_cb}});

  ClientOptions options;
  // Speed up client retries.
  options.timer_period = std::chrono::milliseconds(1);
  options.backoff_distribution = [](ClientRNG*) { return 0.0; };
  auto client = CreateClient(std::move(options));

  // Disable communication.
  config_->SetCopilot(HostId());

  auto sub = [&](Topic topic_name, SequenceNumber start_seqno) {
    ASSERT_EQ(0, subscriptions.count(topic_name));
    auto sub_handle = client->Subscribe(
        GuestTenant,
        GuestNamespace,
        topic_name,
        start_seqno,
        nullptr,
        [&](const SubscriptionStatus&) { unsubscribe_sem.Post(); });
    ASSERT_TRUE(sub_handle);
    subscriptions[topic_name] = {start_seqno, sub_handle};
  };

  auto unsub = [&](Topic topic_name) {
    auto it = subscriptions.find(topic_name);
    ASSERT_TRUE(it != subscriptions.end());
    ASSERT_OK(client->Unsubscribe(it->second.second));
    subscriptions.erase(it);
    ASSERT_TRUE(unsubscribe_sem.TimedWait(positive_timeout));
  };

  // Simulate some subscriptions and unsubscriptions.
  sub("a", 0);
  sub("b", 1);
  unsub("a");
  unsub("b");
  // No subscriptions at this point.
  sub("c", 2);
  sub("a", 3);
  sub("b", 4);
  unsub("a");

  // Finished offline operations.
  expects_request = true;
  // Enable communication and wait for Copilot to verify all received
  // requests.
  config_->SetCopilot(copilot.msg_loop->GetHostId());
  ASSERT_TRUE(all_ok_sem.TimedWait(positive_timeout));
}

TEST(ClientTest, CopilotSwap) {
  port::Semaphore subscribe_sem1, subscribe_sem2;
  auto copilot1 =
      MockCopilot({{MessageType::mSubscribe,
                    [&](std::unique_ptr<Message> msg, StreamID origin) {
                      subscribe_sem1.Post();
                    }}});

  ClientOptions options;
  options.timer_period = std::chrono::milliseconds(1);
  // Make client retries utterly slow, to make sure that we switch host
  // immediately.
  options.backoff_initial = std::chrono::seconds(30);
  options.backoff_limit = std::chrono::seconds(30);
  options.backoff_distribution = [](ClientRNG*) { return 0.0; };
  auto client = CreateClient(std::move(options));

  // Subscribe, call should make it to the copilot.
  client->Subscribe(GuestTenant, GuestNamespace, "CopilotSwap", 0);
  ASSERT_TRUE(subscribe_sem1.TimedWait(positive_timeout));

  // Launch another copilot, this will automatically update configuration.
  auto copilot2 =
      MockCopilot({{MessageType::mSubscribe,
                    [&](std::unique_ptr<Message> msg, StreamID origin) {
                      subscribe_sem2.Post();
                    }}});
  ASSERT_TRUE(subscribe_sem2.TimedWait(positive_timeout));
}

TEST(ClientTest, NoPilot) {
  port::Semaphore publish_sem;
  auto client = CreateClient(ClientOptions());

  // Publish (without pilot), call should invoke callback with error.
  auto ps = client->Publish(GuestTenant,
                            "NoPilot",
                            GuestNamespace,
                            TopicOptions(),
                            "data",
                            [&] (std::unique_ptr<ResultStatus> rs) {
                              ASSERT_TRUE(!rs->GetStatus().ok());
                              publish_sem.Post();
                            });
  ASSERT_TRUE(ps.status.ok());
  ASSERT_TRUE(publish_sem.TimedWait(negative_timeout));
}

}  // namespace rocketspeed

int main(int argc, char** argv) {
  return rocketspeed::test::RunAllTests();
}
