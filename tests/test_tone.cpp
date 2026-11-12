#include "audio_tone.h"
#include <cassert>
#include <iostream>
#include <vector>

static int16_t unpack(uint8_t lo, uint8_t hi) { return int16_t(uint16_t(lo) | (uint16_t(hi) << 8)); }
static void check_profile(trivia_tone::Profile profile) {
  trivia_tone::requested_profile = profile; trivia_tone::reset();
  std::vector<uint8_t> expected;
  for (size_t n = 0; n < 16000; ++n) {
    const auto value = trivia_tone::sample(n);
    expected.push_back(uint8_t(value)); expected.push_back(uint8_t(uint16_t(value) >> 8));
    if (profile == trivia_tone::Profile::REFERENCE_A) {
      const float fade = std::min(1.0f, std::min(float(n)/160.0f, float(15999-n)/160.0f));
      assert(value == int16_t(3200.0f * fade * sinf(2.0f * 3.14159265f * 440.0f * float(n) / 16000.0f)));
    }
  }
  if (profile == trivia_tone::Profile::HIGH_B) {
    assert(trivia_tone::sample(0) == 0 && trivia_tone::sample(7999) == 0);
    assert(trivia_tone::sample(8000) == 0 && trivia_tone::sample(15999) == 0);
    for (size_t segment = 0; segment < 2; ++segment) {
      int rises = 0, peak = 0; int16_t prev = 0; double sum = 0, square = 0;
      for (size_t n = segment * 8000; n < (segment+1) * 8000; ++n) {
        const int16_t value = unpack(expected[2*n],expected[2*n+1]);
        peak = std::max(peak,std::abs(int(value))); sum += value; square += double(value)*value;
        if (prev <= 0 && value > 0) ++rises; prev = value;
      }
      assert(rises == (segment == 0 ? 500 : 1000));
      assert(peak >= 11990 && peak <= 12000);
      assert(std::abs(sum / 8000) < 3 && std::sqrt(square / 8000) > 8000);
    }
  }
  // A UI request during playback cannot mutate the clip already being generated.
  trivia_tone::requested_profile = profile == trivia_tone::Profile::HIGH_B
      ? trivia_tone::Profile::REFERENCE_A : trivia_tone::Profile::HIGH_B;
  std::vector<uint8_t> delivered; size_t turn = 0;
  while (trivia_tone::offset < trivia_tone::BYTES) {
    const size_t count = trivia_tone::make_chunk();
    const size_t accepted = turn++ % 4 == 0 ? 0 : std::min(count,size_t(126));
    delivered.insert(delivered.end(),trivia_tone::chunk.begin(),trivia_tone::chunk.begin()+accepted);
    trivia_tone::offset += accepted;
  }
  assert(delivered == expected);
}
static void check_write_timing() {
  trivia_tone::WriteTiming t;
  assert(t.min_call_gap() == 0 && t.min_success_gap() == 0);
  t.observe(1000,1100,1024,0); t.observe(10000,10025,1024,126); t.observe(150000,150100,1024,1024);
  assert(t.attempts == 3 && t.successful == 2 && t.zero_accepts == 1 && t.partial_accepts == 1);
  assert(t.min_call_gap() == 9000 && t.call_gap_max == 140000);
  assert(t.min_success_gap() == 140075 && t.success_gap_max == 140075 && t.call_duration_max == 100);
  t = {}; t.observe(0xfffffff0U,0xfffffff5U,1024,1024); t.observe(0x10U,0x20U,1024,1024);
  assert(t.min_call_gap() == 32 && t.min_success_gap() == 43 && t.call_duration_max == 16);
  trivia_tone::requested_profile = trivia_tone::Profile::REFERENCE_A; trivia_tone::reset();
  assert(trivia_tone::timing.attempts == 0 && trivia_tone::timing.min_success_gap() == 0);
}
int main() {
  // The old generator formula itself is not silent or a wrong-frequency waveform.
  int old_peak=0, old_rises=0; double old_square=0; int16_t old_prev=0;
  for (size_t n=0;n<3200;++n) {
    const int16_t value=int16_t(3200.0f*sinf(2.0f*3.14159265f*440.0f*float(n)/16000.0f));
    const auto decoded=unpack(uint8_t(value),uint8_t(uint16_t(value)>>8));
    assert(value==decoded); old_peak=std::max(old_peak,std::abs(int(value))); old_square+=double(value)*value;
    if(old_prev<=0 && value>0)++old_rises; old_prev=value;
  }
  assert(old_peak==3200 && old_rises==88 && std::sqrt(old_square/3200)>2200);
  std::vector<uint8_t> expected;
  for(size_t n=0;n<trivia_tone::SAMPLES;++n) {
    auto value=trivia_tone::sample(n); expected.push_back(uint8_t(value)); expected.push_back(uint8_t(uint16_t(value)>>8));
  }
  assert(expected.size()==32000 && trivia_tone::sample(0)==0 && trivia_tone::sample(15999)==0);
  int rises=0,peak=0; int16_t prev=0; double sum=0,square=0;
  for(size_t n=0;n<16000;++n) {
    int16_t value=unpack(expected[2*n],expected[2*n+1]); peak=std::max(peak,std::abs(int(value)));
    sum+=value;square+=double(value)*value;if(prev<=0 && value>0)++rises;prev=value;
  }
  assert(rises==440 && peak==3200 && std::abs(sum/16000)<2 && std::sqrt(square/16000)>2200);
  // A series of zero/short accepts must preserve phase and every byte exactly.
  trivia_tone::reset();std::vector<uint8_t> delivered;size_t turn=0;
  while(trivia_tone::offset<trivia_tone::BYTES) {
    const size_t count=trivia_tone::make_chunk();assert(count<=1024 && count%2==0);
    const size_t accepted=(turn++%3==0)?0:std::min(count,size_t(126));
    delivered.insert(delivered.end(),trivia_tone::chunk.begin(),trivia_tone::chunk.begin()+accepted);
    trivia_tone::offset+=accepted;
  }
  assert(delivered==expected && trivia_tone::make_chunk()==0);
  // A later hard stop must not convert an incomplete drain into a false pass.
  trivia_tone::reset();trivia_tone::all_queued=true;trivia_tone::dac_frames.store(16000);
  assert(!trivia_tone::complete(true));
  trivia_tone::drained=true;assert(trivia_tone::complete(true));
  trivia_tone::dac_frames.store(15999);assert(!trivia_tone::complete(true));
  trivia_tone::dac_frames.store(16000);assert(!trivia_tone::complete(false));
  trivia_tone::failed=true;assert(!trivia_tone::complete(true));
  check_profile(trivia_tone::Profile::REFERENCE_A);
  check_profile(trivia_tone::Profile::HIGH_B);
  check_write_timing();
  std::cout<<"PASS: old PCM 440Hz/nonzero; new PCM 16000 samples/32000 bytes, 440Hz, peak3200, fades, LE16 and short-write phase continuity and strict DAC/drain completion; A bit-identical/B 1kHz+2kHz peak12000; short-write/latch/timing/wrap tests\n";
}
