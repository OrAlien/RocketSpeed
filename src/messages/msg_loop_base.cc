//  Copyright (c) 2014, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
#define __STDC_FORMAT_MACROS
#include "msg_loop_base.h"

#include <chrono>
#include <thread>

#include "src/port/port.h"
#include "src/util/common/base_env.h"
#include "src/util/common/statistics.h"

namespace rocketspeed {

Statistics MsgLoopBase::AggregateStatsSync(WorkerStatsProvider stats_provider) {
  Statistics aggregated_stats;
  port::Semaphore done;
  Status st;
  do {
    // Attempt to gather num clients from each event loop.
    st = Gather(stats_provider,
      [&done, &aggregated_stats] (std::vector<Statistics> clients) {
        for (Statistics& stat: clients) {
          aggregated_stats.Aggregate(stat.MoveThread());
        }
        done.Post();
      });
    if (!st.ok()) {
      // Failed, delay for a while.
      /* sleep override */
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  } while (!st.ok());
  done.Wait();
  return aggregated_stats.MoveThread();
}

}  // namespace rocketspeed
