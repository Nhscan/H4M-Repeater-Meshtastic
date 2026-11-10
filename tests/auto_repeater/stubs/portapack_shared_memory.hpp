#pragma once
#include <cstdint>
#include <vector>
struct SubGhzDDataMessage {uint8_t sensorType;uint16_t bits;uint64_t data;};
struct TestQueue {std::vector<SubGhzDDataMessage> packets{};void push(const SubGhzDDataMessage& p){packets.push_back(p);}};
struct TestShared {TestQueue application_queue{};};
inline TestShared shared_memory{};
