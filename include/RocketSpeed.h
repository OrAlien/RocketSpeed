// Copyright (c) 2014, Facebook, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <random>
#include <vector>

#include "include/Slice.h"
#include "include/Status.h"
#include "include/SubscriptionStorage.h"
#include "include/Types.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC visibility push(default)
#endif
namespace rocketspeed {

class BaseEnv;
class DataLossInfo;
class Logger;
class WakeLock;

/** Notifies about the staus of a message published to the RocketSpeed. */
typedef std::function<void(std::unique_ptr<ResultStatus>)> PublishCallback;

/**
 * Notifies the application about status of a subscription, see subscribe call
 * for details.
 */
typedef std::function<void(const SubscriptionStatus&)> SubscribeCallback;

/**
 * Notifies about message received on a subscription.
 *
 * The application can steal message from the reference argument, but if it
 * doesn't, using the message object after the callback yields undefined
 * behaviour.
 * If the application does not need message payload buffer outside of the
 * callback, it is advised not to steal the message object.
 */
typedef std::function<void(std::unique_ptr<MessageReceived>&)>
    MessageReceivedCallback;

typedef std::function<void(std::unique_ptr<DataLossInfo>&)>
    DataLossCallback;

/** Notifies about status of a finished subscription snapshot. */
typedef std::function<void(Status)> SaveSubscriptionsCallback;

/** Client random number generator. */
typedef std::mt19937_64 ClientRNG;

/** Describes the Client object to be created. */
class ClientOptions {
 public:
  // Configuration of this service provider.
  std::shared_ptr<Configuration> config;

  // Logger that is used for info messages.
  // Default: nullptr.
  std::shared_ptr<Logger> info_log;

  // BaseEnv to be used by all client side components.
  // Default: a default env of a client.
  BaseEnv* env;

  // WakeLock acquired by the client when it processes messages.
  // Default: nullptr
  std::shared_ptr<WakeLock> wake_lock;

  // Number of threads used by the client.
  // Default: 1
  int num_workers;

  // Strategy for storing subscriptions.
  // Default: null-strategy.
  std::unique_ptr<SubscriptionStorage> storage;

  // Determines whether terminating the last subscription on a connection should
  // trigger disconnection.
  // Default: true
  bool close_connection_with_no_subscription;

  // Period of internal client clock. Determines resolution of backoff and rate
  // limiting time measurements.
  // Default: 200 ms
  std::chrono::milliseconds timer_period;

  // A base of exponential back-off curve.
  // Default: 2.0
  double backoff_base;

  // A scaling factor of the exponential back-off curve.
  // Default: 1 s
  std::chrono::milliseconds backoff_initial;

  // A limit on back-off period.
  // Default: 30 s
  std::chrono::milliseconds backoff_limit;

  // A distribution used to determine final back-off period. If P is the period
  // calculated according to truncated exponential back-off algorithm, the final
  // period equals distribution(rng) * P.
  // Default: std::uniform_real_distribution<double>
  typedef std::function<double(ClientRNG*)> BackOffDistribution;
  BackOffDistribution backoff_distribution;

  // If client receives a burst of messages on non-existent subscription ID, it
  // will respond with unsubscribe message only once every specified duration.
  // Default: 10s
  std::chrono::milliseconds unsubscribe_deduplication_timeout;

  /** Creates options with default values. */
  ClientOptions();
};

/** The Client is used to produce and consume messages on arbitrary topics. */
class Client {
 public:
  /**
   * Creates the client object.
   *
   * @param options The description of client object to be created.
   * @return Status::OK() iff client was created successfully.
   */
  static Status Create(ClientOptions client_options,
                       std::unique_ptr<Client>* client);

  /** Closes the client, connections and frees all allocated resources. */
  virtual ~Client() = default;

  /**
   * Sets default callback for announcing subscription status and message
   * delivery.
   *
   * This method is NOT thread-safe, if ever called, it must be right after
   * creating the client from the same thread. Caller must ensure that this call
   * "happened-before" any call which creates a new subscription.
   *
   * @param subscription_callback See docs for corresponding callback in
   *                              subscribe method.
   * @param deliver_callback See docs for corresponding callback in
   *                         subscribe method.
   * @param data_loss_callback See docs for corresponding callback in
   *                           subscribe method.
   */
  virtual void SetDefaultCallbacks(
      SubscribeCallback subscription_callback = nullptr,
      MessageReceivedCallback deliver_callback = nullptr,
      DataLossCallback data_loss_callback = nullptr) = 0;

  /**
   * Asynchronously publishes a new message to the Topic. The return parameter
   * indicates whether the publish was successfully enqueued.
   *
   * @param tenant_id ID of tenant responsible for the publish.
   * @param topic_name Name of this topic to be opened
   * @param topic_namespace Namespace of this topic name
   * @param options Quality of service for this Topic
   * @param data Payload of message
   * @param callback Callback to call with response from RocketSpeed. This will
   *                 only be called when Publish was successful. The result will
   *                 be ok if the message was succesfully commited into
   *                 RocketSpeed, otherwise an error will be provided.
   * @params message_id The provided message_id, optional
   * @return the status and message ID of the published message.
   */
  virtual PublishStatus Publish(const TenantID tenant_id,
                                const Topic& topic_name,
                                const NamespaceID& topic_namespace,
                                const TopicOptions& options,
                                const Slice& data,
                                PublishCallback callback = nullptr,
                                const MsgId message_id = MsgId()) = 0;

  /**
   * Subscribes to a topic with provided parameters.
   *
   * Subscribe callback is invoked when the client is successfully subscribed.
   * Messages arriving on this subscription are delivered to the application via
   * message received callback.
   *
   * @param parameters Parameters of the subscription.
   * @param deliver_callback Invoked with every message received on the
   *                         subscription.
   * @param subscription_callback Invoked to notify termination of the
   *                              subscription.
   * @param data_loss_callback Invoked to notify there's been data loss.
   * @return A handle that identifies this subscription. The handle is unengaged
   *         iff the Client failed to create the subscription.
   */
  virtual SubscriptionHandle Subscribe(
      SubscriptionParameters parameters,
      MessageReceivedCallback deliver_callback = nullptr,
      SubscribeCallback subscription_callback = nullptr,
      DataLossCallback data_loss_callback = nullptr) = 0;

  /** Convenience method, see the other overload for details. */
  virtual SubscriptionHandle Subscribe(
      TenantID tenant_id,
      NamespaceID namespace_id,
      Topic topic_name,
      SequenceNumber start_seqno,
      MessageReceivedCallback deliver_callback = nullptr,
      SubscribeCallback subscription_callback = nullptr,
      DataLossCallback data_loss_callback = nullptr) = 0;

  /**
   * Unsubscribes from a topic identified by provided handle.
   *
   * Subscribe callback is invoked when the client is successfully unsubscribed.
   * Messages arriving on this subscription after client unsubscribed, are
   * silently dropped.
   *
   * @param A handle that identifies the subscription.
   * @return Status::OK() iff unsubscription request was successfully enqueued.
   */
  virtual Status Unsubscribe(SubscriptionHandle sub_handle) = 0;

  /**
   * Acknowledges that this message was processed by the application.
   * All sequence numbers no later than the sequence number of given message
   * are as well considered to be processed by the application. When application
   * saves, and then restores subscription state, the subscription will continue
   * from the next sequence number.
   *
   * @param message The message to be acknowledged.
   * @return Status::OK() iff acknowledgement was successful.
   */
  virtual Status Acknowledge(const MessageReceived& message) = 0;

  /**
   * Saves state of subscriptions according to strategy selected when opening
   * the client.
   * All messages acknowledged before this call will be included in the saved
   * state, which means that the application will be able to restore them using
   * the following call.
   *
   * @param save_callback A callback to inform whether saving succeeded.
   */
  virtual void SaveSubscriptions(SaveSubscriptionsCallback save_callback) = 0;

  /**
   * Reads all subscriptions saved by strategy selected when opening the client.
   * Returns a list with subscription prototypes, so that an application can
   * decide which subscriptions should be restored and provide appropriate
   * callbacks.
   *
   * @subscriptions An out parameter with a list of restored subscriptions.
   * @return Status::OK iff subscriptions were restored successfully.
   */
  virtual Status RestoreSubscriptions(
      std::vector<SubscriptionParameters>* subscriptions) = 0;
};

}  // namespace rocketspeed
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC visibility pop
#endif
