// SPDX-License-Identifier: Apache License 2.0 OR GPL-2.0
#pragma once

#include <iostream>
#include <unordered_map>
#include <fstream>

#include "metrics.h"
#include "port/port.h"
#include "util/mutexlock.h"

namespace ROCKSDB_NAMESPACE {

const std::unordered_map<uint32_t, std::pair<std::string, uint32_t>>
    ZenFSHistogramsNameMap = {
        {ZENFS_WRITE_QPS, {"zenfs_write_qps", ZENFS_REPORTER_TYPE_QPS}},
        {ZENFS_READ_QPS, {"zenfs_read_qps", ZENFS_REPORTER_TYPE_QPS}}};

struct ReporterSample {
 public:
  typedef uint64_t TypeTime;
  typedef uint64_t TypeValue;
  typedef std::pair<TypeTime, TypeValue> TypeRecord;

 private:
  port::Mutex mu_;
  ZenFSMetricsReporterType type_;
  ZenFSMetricsHistograms histograms_;
  std::vector<TypeRecord> hist_;

  static const TypeTime MinReportInterval = 1;

  bool ReadyToReport(uint64_t time) const {
    // AssertHeld(&mu);
    if (hist_.size() == 0) return 1;
    TypeTime last_report_time =
        hist_.rbegin()->first;  
    return time > last_report_time + MinReportInterval;
  }

 public:
  ReporterSample(ZenFSMetricsReporterType type, ZenFSMetricsHistograms histograms) : mu_(), type_(type), histograms_(histograms), hist_() {}

  void Record(const TypeTime& time, TypeValue value) {
    MutexLock guard(&mu_);
    if (ReadyToReport(time)) hist_.push_back(TypeRecord(time, value));
  }

  ZenFSMetricsReporterType GetType() const { return type_; }
  ZenFSMetricsHistograms GetHistograms() const { return histograms_; }
  std::vector<TypeRecord> GetHist() const { return hist_; }
  uint64_t GetHistSize() const { return static_cast<uint64_t>(hist_.size()); }
  void ClearHist() { hist_.clear(); } 

  void GetHistSnapshot(std::vector<TypeRecord>& hist) {
    MutexLock guard(&mu_);
    hist = hist_;
  }
};

struct ZenFSMetricsQps : public ZenFSMetrics {
 private:
  Env* env_;
  std::vector<ReporterSample *> reporter_qps_;

 public:
  ZenFSMetricsQps(Env* env) : env_(env) {
    reporter_qps_.push_back(new ReporterSample(ZENFS_REPORTER_TYPE_QPS, ZENFS_WRITE_QPS));
    reporter_qps_.push_back(new ReporterSample(ZENFS_REPORTER_TYPE_QPS, ZENFS_READ_QPS));
  }
  ~ZenFSMetricsQps() {
    for (auto& reporter : reporter_qps_) delete reporter;
  }

  virtual void AddReporter(uint32_t label_uint,
                           uint32_t type_uint = 0) override {
    auto label = static_cast<ZenFSMetricsHistograms>(label_uint);
    assert(ZenFSHistogramsNameMap.find(label) != ZenFSHistogramsNameMap.end());

    auto pair = ZenFSHistogramsNameMap.find(label)->second;
    auto type = pair.second;

    if (type_uint != 0) {
      auto type_check = static_cast<ZenFSMetricsReporterType>(type_uint);
      assert(type_check == type);
      (void)type_check;
    }
    (void)type;
  }

  virtual void Report(uint32_t label_uint, size_t value,
                      uint32_t type_uint) override {
    auto label = static_cast<ZenFSMetricsHistograms>(label_uint);
    assert(label == ZENFS_WRITE_QPS || label == ZENFS_READ_QPS);
    auto type = static_cast<ZenFSMetricsReporterType>(type_uint);
    assert(type == ZENFS_REPORTER_TYPE_QPS);
    (void)type;

    auto it = std::find_if(reporter_qps_.begin(), reporter_qps_.end(),
                           [label](const ReporterSample *reporter) {
                             return reporter->GetHistograms() == label;
                           });
    assert(it != reporter_qps_.end());
    (*it)->Record(GetTime(), value);
  }

  virtual void ReportSnapshot(const ZenFSSnapshot& /*snapshot*/) override {}

 public:
  virtual void ReportQPS(uint32_t label, size_t qps) override {
    Report(label, qps, ZENFS_REPORTER_TYPE_QPS);
  }

 public:
  uint64_t GetQps() const {
    uint64_t qps = 0;
    for (auto& reporter : reporter_qps_) qps += reporter->GetHistSize();
    return qps;
  }

  uint64_t GetWriteQps() const {
    for (auto& reporter : reporter_qps_) {
      if (reporter->GetHistograms() == ZENFS_WRITE_QPS)
        return reporter->GetHistSize();
    }
    return 0;
  }

  uint64_t GetReadQps() const {
    for (auto& reporter : reporter_qps_) {
      if (reporter->GetHistograms() == ZENFS_READ_QPS)
        return reporter->GetHistSize();
    }
    return 0;
  }

  void ClearQps() {
    for (auto& reporter : reporter_qps_) reporter->ClearHist();
  }
  
  void DebugQpsPrint() {
    std::ofstream os;
    os.open("/home/zln/qps.txt", std::ios::out | std::ios::app); 
    if (!os.is_open()) {
      printf("Open qps file failed");
      return;
    }
    os << GetWriteQps() << " " << GetReadQps() << std::endl;
    ClearQps();
    os.close();
  }

 private:
  uint64_t GetTime() {
    return env_->NowMicros();
  }  
};

}  // namespace ROCKSDB_NAMESPACE
