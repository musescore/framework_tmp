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

#ifndef MUSE_MUSESAMPLER_MUSESAMPLERRESOLVER_H
#define MUSE_MUSESAMPLER_MUSESAMPLERRESOLVER_H

#include "audio/worker/isynthresolver.h"
#include "modularity/ioc.h"

#include "libhandler.h"
#include "imusesamplerconfiguration.h"
#include "imusesamplerinfo.h"

#include "async/notification.h"

namespace muse::musesampler {
class MuseSamplerResolver : public audio::synth::ISynthResolver::IResolver, public IMuseSamplerInfo, public Injectable
{
    Inject<IMuseSamplerConfiguration> configuration = { this };

public:

    MuseSamplerResolver(const modularity::ContextPtr& iocCtx)
        : Injectable(iocCtx) {}

    void init();

    bool reloadAllInstruments();
    void processOnlineSounds();

    int buildNumber() const;

    muse::audio::synth::ISynthesizerPtr resolveSynth(const muse::audio::TrackId trackId,
                                                     const muse::audio::AudioInputParams& params) const override;
    bool hasCompatibleResources(const muse::audio::PlaybackSetupData& setup) const override;
    muse::audio::AudioResourceMetaList resolveResources() const override;
    muse::audio::SoundPresetList resolveSoundPresets(const muse::audio::AudioResourceMeta& resourceMeta) const override;
    void refresh() override;
    void clearSources() override;

    std::string version() const override;
    bool isInstalled() const override;

    float defaultReverbLevel(const String& instrumentSoundId) const override;

    ByteArray drumMapping(int instrumentId) const override;
    std::vector<Instrument> instruments() const override;

private:
    void loadSoundPresetAttributes(muse::audio::SoundPresetAttributes& attributes, int instrumentId, const char* presetCode) const;

    String buildMuseInstrumentId(const String& category, const String& name, int uniqueId) const;

    MuseSamplerLibHandlerPtr m_libHandler = nullptr;
    async::Notification m_processOnlineSoundsRequested;
};
}

#endif // MUSE_MUSESAMPLER_MUSESAMPLERRESOLVER_H
