#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>
#include <cmath>

// ============================================================
// JunoArpeggiator
// ============================================================
enum ArpDivision
{
    kDiv1 = 0,   // whole note
    kDiv2,       // 1/2 note
    kDiv4,       // 1/4 note
    kDiv4T,      // 1/4 triplet
    kDiv8,       // 1/8 note
    kDiv8T,      // 1/8 triplet
    kDiv16,      // 1/16 note
    kDiv16T,     // 1/16 triplet
    kDiv32,      // 1/32 note
    kNumArpDivisions
};

static constexpr double kDivBeats[kNumArpDivisions] = {
    4.0,         // 1/1
    2.0,         // 1/2
    1.0,         // 1/4
    2.0 / 3.0,   // 1/4T
    0.5,         // 1/8
    1.0 / 3.0,   // 1/8T
    0.25,        // 1/16
    1.0 / 6.0,   // 1/16T
    0.125        // 1/32
};

class JunoArpeggiator
{
public:
    bool mEnabled = false;
    int mMode = 0;       // 0=Up, 1=Up/Down, 2=Down
    int mRange = 0;      // 0=1oct, 1=2oct, 2=3oct
    float mRate = 120.f; // BPM (steps per minute)
    float mSampleRate = 44100.f;

    // DAW sync state
    bool mSyncToHost = false;
    bool mHostPlaying = false;
    double mHostBPM = 120.0;
    double mHostBeatPos = 0.0;
    int mDivision = kDiv16;
    int64_t mLastSyncStep = -1;

    std::vector<int> mHeldNotes; // sorted ascending

    int mStepIndex = 0;
    int mDirection = 1;  // 1=ascending, -1=descending
    float mPhase = 0.f;
    int mLastNote = -1;  // currently sounding arp note
    std::atomic<uint32_t> mTickCount{0};

    // Limits to physical keyboard (MIDI 36-96)
    static constexpr int kMaxArpNote = 96;
    bool mLimitToKeyboard = true;

    JunoArpeggiator()
    {
        mHeldNotes.reserve(128);
    }

    static float arpRate(float t)
    {
        float pos = 1.f - t;
        float hz = 1.0f / (2.0f * (33000.0f + pos * 1000000.0f) * 0.47e-6f * 0.6633f);
        return hz * 60.f;
    }

    void SetSampleRate(float sr) { mSampleRate = sr; }

    void NoteOn(int note)
    {
        if (note < 0 || note > 127) return;
        auto it = std::lower_bound(mHeldNotes.begin(), mHeldNotes.end(), note);
        if (it != mHeldNotes.end() && *it == note) return;
        mHeldNotes.insert(it, note);

        if (mHeldNotes.size() == 1)
        {
            mPhase = 1.f;
            mStepIndex = 0;
            mDirection = 1;
        }
    }

    void NoteOff(int note)
    {
        auto it = std::find(mHeldNotes.begin(), mHeldNotes.end(), note);
        if (it != mHeldNotes.end())
            mHeldNotes.erase(it);

        if (mHeldNotes.empty())
        {
            mStepIndex = 0;
            mDirection = 1;
            mPhase = 0.f;
        }
    }

    void Reset()
    {
        mHeldNotes.clear();
        mStepIndex = 0;
        mDirection = 1;
        mPhase = 0.f;
        mLastNote = -1;
        mLastSyncStep = -1;
    }

    int SeqLen() const
    {
        if (mLimitToKeyboard)
            return static_cast<int>(mHeldNotes.size()) * (mRange + 1);

        int count = 0;
        int octaves = mRange + 1;
        for (int oct = 0; oct < octaves; oct++)
            for (int n : mHeldNotes)
                if (n + oct * 12 <= 127) count++;
        return count;
    }

    int SeqNote(int idx) const
    {
        int i = 0;
        int octaves = mRange + 1;
        for (int oct = 0; oct < octaves; oct++)
            for (int n : mHeldNotes)
            {
                int note = n + oct * 12;
                if (mLimitToKeyboard)
                {
                    while (note > kMaxArpNote) note -= 12;
                }
                else
                {
                    if (note > 127) continue;
                }
                if (i == idx) return note;
                i++;
            }
        return -1;
    }

    int NextNote()
    {
        int len = SeqLen();
        if (len == 0) return -1;

        if (mStepIndex >= len) mStepIndex = 0;
        if (mStepIndex < 0) mStepIndex = len - 1;

        int note;
        switch (mMode)
        {
            case 0: // Up
                note = SeqNote(mStepIndex);
                mStepIndex = (mStepIndex + 1) % len;
                break;

            case 1: // Up/Down
                note = SeqNote(mStepIndex);
                if (len > 1)
                {
                    mStepIndex += mDirection;
                    if (mStepIndex >= len) { mStepIndex = len - 2; mDirection = -1; }
                    else if (mStepIndex < 0) { mStepIndex = 1; mDirection = 1; }
                }
                break;

            case 2: // Down
                note = SeqNote(len - 1 - mStepIndex);
                mStepIndex = (mStepIndex + 1) % len;
                break;

            default:
                note = SeqNote(0);
        }
        return note;
    }

    template <typename NoteOnF, typename NoteOffF>
    void Process(int nFrames, NoteOnF noteOn, NoteOffF noteOff)
    {
        if (!mEnabled || mHeldNotes.empty())
        {
            if (mLastNote >= 0)
            {
                noteOff(mLastNote, 0);
                mLastNote = -1;
            }
            mLastSyncStep = -1;
            return;
        }

        if (mSyncToHost)
        {
            int div = std::max(0, std::min(mDivision, static_cast<int>(kNumArpDivisions) - 1));
            double divBeats = kDivBeats[div];

            if (mHostPlaying)
            {
                double beatsPerSample = mHostBPM / (60.0 * static_cast<double>(mSampleRate));

                for (int s = 0; s < nFrames; s++)
                {
                    double beatPos = mHostBeatPos + s * beatsPerSample;
                    int64_t stepNow = static_cast<int64_t>(std::floor(beatPos / divBeats));

                    if (stepNow != mLastSyncStep)
                    {
                        mLastSyncStep = stepNow;
                        mTickCount.fetch_add(1, std::memory_order_relaxed);

                        if (mLastNote >= 0)
                            noteOff(mLastNote, s);

                        int note = NextNote();
                        if (note >= 0)
                        {
                            noteOn(note, s);
                            mLastNote = note;
                        }
                    }
                }
                return;
            }

            float syncRate = static_cast<float>(mHostBPM / divBeats);
            float inc = syncRate / (60.f * mSampleRate);

            for (int s = 0; s < nFrames; s++)
            {
                mPhase += inc;
                if (mPhase >= 1.f)
                {
                    mPhase -= 1.f;
                    mTickCount.fetch_add(1, std::memory_order_relaxed);

                    if (mLastNote >= 0)
                        noteOff(mLastNote, s);

                    int note = NextNote();
                    if (note >= 0)
                    {
                        noteOn(note, s);
                        mLastNote = note;
                    }
                }
            }
            return;
        }

        float inc = mRate / (60.f * mSampleRate);

        for (int s = 0; s < nFrames; s++)
        {
            mPhase += inc;
            if (mPhase >= 1.f)
            {
                mPhase -= 1.f;
                mTickCount.fetch_add(1, std::memory_order_relaxed);

                if (mLastNote >= 0)
                    noteOff(mLastNote, s);

                int note = NextNote();
                if (note >= 0)
                {
                    noteOn(note, s);
                    mLastNote = note;
                }
            }
        }
    }
};
