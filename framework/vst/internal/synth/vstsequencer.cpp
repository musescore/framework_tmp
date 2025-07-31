/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "vstsequencer.h"

#include "global/interpolation.h"

using namespace muse;
using namespace muse::vst;

static constexpr ControlIdx MODWHEEL_IDX = static_cast<ControlIdx>(Steinberg::Vst::kCtrlModWheel);
static constexpr ControlIdx SUSTAIN_IDX = static_cast<ControlIdx>(Steinberg::Vst::kCtrlSustainOnOff);
static constexpr ControlIdx SOSTENUTO_IDX = static_cast<ControlIdx>(Steinberg::Vst::kCtrlSustenutoOnOff);
static constexpr ControlIdx PITCH_BEND_IDX = static_cast<ControlIdx>(Steinberg::Vst::kPitchBend);

static const mpe::ArticulationTypeSet SUSTAIN_PEDAL_CC_SUPPORTED_TYPES {
    mpe::ArticulationType::Pedal, mpe::ArticulationType::LetRing,
};

static const mpe::ArticulationTypeSet SOSTENUTO_PEDAL_CC_SUPPORTED_TYPES = {
    mpe::ArticulationType::LaissezVibrer,
};

static const mpe::ArticulationTypeSet BEND_SUPPORTED_TYPES {
    mpe::ArticulationType::Multibend,
};

static constexpr mpe::pitch_level_t MIN_SUPPORTED_PITCH_LEVEL = mpe::pitchLevel(mpe::PitchClass::C, 0);
static constexpr int MIN_SUPPORTED_NOTE = 12; // VST equivalent for C0
static constexpr mpe::pitch_level_t MAX_SUPPORTED_PITCH_LEVEL = mpe::pitchLevel(mpe::PitchClass::C, 8);
static constexpr int MAX_SUPPORTED_NOTE = 108; // VST equivalent for C8

void VstSequencer::init(ParamsMapping&& mapping, bool useDynamicEvents)
{
    m_mapping = std::move(mapping);
    m_useDynamicEvents = useDynamicEvents;
    m_inited = true;

    updateMainStreamEvents(m_playbackData.originEvents, m_playbackData.dynamics);
}

void VstSequencer::updateOffStreamEvents(const mpe::PlaybackEventsMap& events, const mpe::DynamicLevelLayers& dynamics)
{
    addPlaybackEvents(m_offStreamEvents, events);

    if (m_useDynamicEvents) {
        addDynamicEvents(m_offStreamEvents, dynamics);
    }

    updateOffSequenceIterator();
}

void VstSequencer::updateMainStreamEvents(const mpe::PlaybackEventsMap& events, const mpe::DynamicLevelLayers& dynamics)
{
    if (!m_inited) {
        return;
    }

    m_mainStreamEvents.clear();

    if (m_onMainStreamFlushed) {
        m_onMainStreamFlushed();
    }

    addPlaybackEvents(m_mainStreamEvents, events);

    if (m_useDynamicEvents) {
        addDynamicEvents(m_mainStreamEvents, dynamics);
    }

    updateMainSequenceIterator();
}

muse::audio::gain_t VstSequencer::currentGain() const
{
    if (m_useDynamicEvents) {
        mpe::dynamic_level_t currentDynamicLevel = dynamicLevel(m_playbackPosition);
        return expressionLevel(currentDynamicLevel);
    }

    return 0.5f;
}

void VstSequencer::addPlaybackEvents(EventSequenceMap& destination, const mpe::PlaybackEventsMap& events)
{
    SostenutoTimeAndDurations sostenutoTimeAndDurations;

    for (const auto& evPair : events) {
        for (const mpe::PlaybackEvent& event : evPair.second) {
            if (std::holds_alternative<mpe::NoteEvent>(event)) {
                addNoteEvent(destination, std::get<mpe::NoteEvent>(event), sostenutoTimeAndDurations);
            } else if (std::holds_alternative<mpe::ControllerChangeEvent>(event)) {
                addControlChangeEvent(destination, evPair.first, std::get<mpe::ControllerChangeEvent>(event));
            }
        }
    }

    addSostenutoEvents(destination, sostenutoTimeAndDurations);
}

void VstSequencer::addDynamicEvents(EventSequenceMap& destination, const mpe::DynamicLevelLayers& layers)
{
    for (const auto& layer : layers) {
        for (const auto& dynamic : layer.second) {
            destination[dynamic.first].emplace(expressionLevel(dynamic.second));
        }
    }
}

void VstSequencer::addNoteEvent(EventSequenceMap& destination, const mpe::NoteEvent& noteEvent,
                                SostenutoTimeAndDurations& sostenutoTimeAndDurations)
{
    const mpe::ArrangementContext& arrangementCtx = noteEvent.arrangementCtx();
    const int32_t noteId = noteIndex(noteEvent.pitchCtx().nominalPitchLevel);
    const float velocityFraction = noteVelocityFraction(noteEvent);
    const float tuning = noteTuning(noteEvent, noteId);

    if (arrangementCtx.hasStart()) {
        destination[arrangementCtx.actualTimestamp].emplace(buildEvent(VstEvent::kNoteOnEvent, noteId, velocityFraction, tuning));
    }

    if (arrangementCtx.hasEnd()) {
        const mpe::timestamp_t timestampTo = arrangementCtx.actualTimestamp + noteEvent.arrangementCtx().actualDuration;
        destination[timestampTo].emplace(buildEvent(VstEvent::kNoteOffEvent, noteId, velocityFraction, tuning));
    }

    for (const auto& articPair : noteEvent.expressionCtx().articulations) {
        const mpe::ArticulationMeta& meta = articPair.second.meta;

        if (muse::contains(BEND_SUPPORTED_TYPES, meta.type)) {
            addPitchCurve(destination, noteEvent, meta);
            continue;
        }

        if (muse::contains(SUSTAIN_PEDAL_CC_SUPPORTED_TYPES, meta.type)) {
            addParamChange(destination, meta.timestamp, SUSTAIN_IDX, 1);
            addParamChange(destination, meta.timestamp + meta.overallDuration, SUSTAIN_IDX, 0);
            continue;
        }

        if (muse::contains(SOSTENUTO_PEDAL_CC_SUPPORTED_TYPES, meta.type)) {
            const mpe::timestamp_t timestamp = arrangementCtx.actualTimestamp + noteEvent.arrangementCtx().actualDuration * 0.1; // add offset for Sostenuto to take effect
            sostenutoTimeAndDurations.push_back(mpe::TimestampAndDuration { timestamp, meta.overallDuration });
            continue;
        }
    }
}

void VstSequencer::addControlChangeEvent(EventSequenceMap& destination, const mpe::timestamp_t timestamp,
                                         const mpe::ControllerChangeEvent& event)
{
    switch (event.type) {
    case mpe::ControllerChangeEvent::Modulation:
        addParamChange(destination, timestamp, MODWHEEL_IDX, event.val);
        break;
    case mpe::ControllerChangeEvent::SustainPedalOnOff:
        addParamChange(destination, timestamp, SUSTAIN_IDX, event.val);
        break;
    case mpe::ControllerChangeEvent::PitchBend:
        addParamChange(destination, timestamp, PITCH_BEND_IDX, event.val);
        break;
    case mpe::ControllerChangeEvent::Undefined:
        break;
    }
}

void VstSequencer::addParamChange(EventSequenceMap& destination, const mpe::timestamp_t timestamp,
                                  const ControlIdx controlIdx, const PluginParamValue value)
{
    auto controlIt = m_mapping.find(controlIdx);
    if (controlIt == m_mapping.cend()) {
        return;
    }

    destination[timestamp].emplace(ParamChangeEvent { controlIt->second, value });
}

void VstSequencer::addPitchCurve(EventSequenceMap& destination, const mpe::NoteEvent& noteEvent,
                                 const mpe::ArticulationMeta& artMeta)
{
    auto pitchBendIt = m_mapping.find(PITCH_BEND_IDX);
    if (pitchBendIt == m_mapping.cend() || noteEvent.pitchCtx().pitchCurve.empty()) {
        return;
    }

    const mpe::timestamp_t noteTimestampTo = noteEvent.arrangementCtx().actualTimestamp + noteEvent.arrangementCtx().actualDuration;
    const mpe::timestamp_t pitchBendTimestampTo = std::min(artMeta.timestamp + artMeta.overallDuration, noteTimestampTo);

    ParamChangeEvent event;
    event.paramId = pitchBendIt->second;
    event.value = 0.5f;
    destination[pitchBendTimestampTo].insert(event);

    auto currIt = noteEvent.pitchCtx().pitchCurve.cbegin();
    auto nextIt = std::next(currIt);
    auto endIt = noteEvent.pitchCtx().pitchCurve.cend();

    auto makePoint = [](mpe::timestamp_t time, float value) {
        return Interpolation::Point { static_cast<double>(time), value };
    };

    for (; nextIt != endIt; currIt = nextIt, nextIt = std::next(currIt)) {
        float currBendValue = pitchBendLevel(currIt->second);
        float nextBendValue = pitchBendLevel(nextIt->second);

        mpe::timestamp_t currTime = artMeta.timestamp + artMeta.overallDuration * mpe::percentageToFactor(currIt->first);
        mpe::timestamp_t nextTime = artMeta.timestamp + artMeta.overallDuration * mpe::percentageToFactor(nextIt->first);

        Interpolation::Point p0 = makePoint(currTime, currBendValue);
        Interpolation::Point p1 = makePoint(nextTime, currBendValue);
        Interpolation::Point p2 = makePoint(nextTime, nextBendValue);

        //! NOTE: Increasing this number results in fewer points being interpolated
        constexpr mpe::pitch_level_t POINT_WEIGHT = mpe::PITCH_LEVEL_STEP / 5;
        size_t pointCount = std::abs(nextIt->second - currIt->second) / POINT_WEIGHT;
        pointCount = std::max(pointCount, size_t(1));

        std::vector<Interpolation::Point> points = Interpolation::quadraticBezierCurve(p0, p1, p2, pointCount);

        for (const Interpolation::Point& point : points) {
            mpe::timestamp_t time = static_cast<mpe::timestamp_t>(std::round(point.x));
            if (time < pitchBendTimestampTo) {
                float bendValue = static_cast<float>(point.y);
                event.value = bendValue;
                destination[time].insert(event);
            }
        }
    }
}

void VstSequencer::addSostenutoEvents(EventSequenceMap& destination, const SostenutoTimeAndDurations& sostenutoTimeAndDurations)
{
    for (size_t i = 0; i < sostenutoTimeAndDurations.size(); ++i) {
        const mpe::TimestampAndDuration& currentTnD = sostenutoTimeAndDurations.at(i);
        const mpe::timestamp_t timestampTo = currentTnD.timestamp + currentTnD.duration;

        addParamChange(destination, currentTnD.timestamp, SOSTENUTO_IDX, 1);

        if (i == sostenutoTimeAndDurations.size() - 1) {
            addParamChange(destination, timestampTo, SOSTENUTO_IDX, 0);
            continue;
        }

        const mpe::TimestampAndDuration& nextTnD = sostenutoTimeAndDurations.at(i + 1);
        if (timestampTo <= nextTnD.timestamp) { // handle potential overlap
            addParamChange(destination, timestampTo, SOSTENUTO_IDX, 0);
        }
    }
}

VstEvent VstSequencer::buildEvent(const VstEvent::EventTypes type, const int32_t noteIdx, const float velocityFraction,
                                  const float tuning) const
{
    VstEvent result;

    result.busIndex = 0;
    result.sampleOffset = 0;
    result.ppqPosition = 0;
    result.flags = VstEvent::kIsLive;
    result.type = type;

    if (type == VstEvent::kNoteOnEvent) {
        result.noteOn.noteId = -1;
        result.noteOn.channel = 0;
        result.noteOn.pitch = noteIdx;
        result.noteOn.tuning = tuning;
        result.noteOn.velocity = velocityFraction;
    } else {
        result.noteOff.noteId = -1;
        result.noteOff.channel = 0;
        result.noteOff.pitch = noteIdx;
        result.noteOff.tuning = tuning;
        result.noteOff.velocity = velocityFraction;
    }

    return result;
}

int32_t VstSequencer::noteIndex(const mpe::pitch_level_t pitchLevel) const
{
    if (pitchLevel <= MIN_SUPPORTED_PITCH_LEVEL) {
        return MIN_SUPPORTED_NOTE;
    }

    if (pitchLevel >= MAX_SUPPORTED_PITCH_LEVEL) {
        return MAX_SUPPORTED_NOTE;
    }

    float stepCount = MIN_SUPPORTED_NOTE + ((pitchLevel - MIN_SUPPORTED_PITCH_LEVEL) / static_cast<float>(mpe::PITCH_LEVEL_STEP));

    return stepCount;
}

float VstSequencer::noteTuning(const mpe::NoteEvent& noteEvent, const int noteIdx) const
{
    int semitonesCount = noteIdx - MIN_SUPPORTED_NOTE;

    mpe::pitch_level_t tuningPitchLevel = noteEvent.pitchCtx().nominalPitchLevel - (semitonesCount * mpe::PITCH_LEVEL_STEP);

    return (tuningPitchLevel / static_cast<float>(mpe::PITCH_LEVEL_STEP)) * 100.f;
}

float VstSequencer::noteVelocityFraction(const mpe::NoteEvent& noteEvent) const
{
    const mpe::ExpressionContext& expressionCtx = noteEvent.expressionCtx();

    if (expressionCtx.velocityOverride.has_value()) {
        return std::clamp(expressionCtx.velocityOverride.value(), 0.f, 1.f);
    }

    mpe::dynamic_level_t dynamicLevel = expressionCtx.expressionCurve.maxAmplitudeLevel();
    return expressionLevel(dynamicLevel);
}

float VstSequencer::expressionLevel(const mpe::dynamic_level_t dynamicLevel) const
{
    static constexpr mpe::dynamic_level_t MIN_SUPPORTED_DYNAMIC_LEVEL = mpe::dynamicLevelFromType(mpe::DynamicType::ppp);
    static constexpr mpe::dynamic_level_t MAX_SUPPORTED_DYNAMIC_LEVEL = mpe::dynamicLevelFromType(mpe::DynamicType::fff);
    static constexpr mpe::dynamic_level_t AVAILABLE_RANGE = MAX_SUPPORTED_DYNAMIC_LEVEL - MIN_SUPPORTED_DYNAMIC_LEVEL;

    if (dynamicLevel <= MIN_SUPPORTED_DYNAMIC_LEVEL) {
        return (0.5f * mpe::ONE_PERCENT) / AVAILABLE_RANGE;
    }

    if (dynamicLevel >= MAX_SUPPORTED_DYNAMIC_LEVEL) {
        return 1.f;
    }

    return RealRound((dynamicLevel - MIN_SUPPORTED_DYNAMIC_LEVEL) / static_cast<float>(AVAILABLE_RANGE), 2);
}

float VstSequencer::pitchBendLevel(const mpe::pitch_level_t pitchLevel) const
{
    static constexpr float SEMITONE_RANGE = 2.f;
    static constexpr float PITCH_BEND_SEMITONE_STEP = 0.5f / SEMITONE_RANGE;

    float pitchLevelSteps = pitchLevel / static_cast<float>(mpe::PITCH_LEVEL_STEP);
    float offset = pitchLevelSteps * PITCH_BEND_SEMITONE_STEP;

    return std::clamp(0.5f + offset, 0.f, 1.f);
}
