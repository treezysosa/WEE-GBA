#include "SampleLoader.h"
namespace wee {
void SampleLoader::run() {
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    while (!threadShouldExit()) {
        exchange.collect();
        juce::String path; uint64_t id = 0;
        {
            const juce::ScopedLock lock(mutex);
            if (requestId != handledId) { id = requestId; path = requestedPath; }
        }
        if (!id) { wait(20); continue; }
        auto asset = std::make_unique<SampleData>();
        juce::String result = "No sample loaded";
        std::vector<float> preview;
        try {
            if (path.isNotEmpty()) {
                std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(path)));
                if (!reader) result = "Cannot read sample. Use LOAD SAMPLE to locate a WAV, AIFF or FLAC file.";
                else if (reader->lengthInSamples < 2 || reader->sampleRate < 8000 || reader->sampleRate > 192000 || reader->numChannels < 1 || reader->numChannels > 2 ||
                         reader->lengthInSamples > juce::int64(reader->sampleRate * 120) || reader->lengthInSamples > 16 * 1024 * 1024)
                    result = "Unsupported sample: use mono/stereo, 8–192 kHz, <=120 s, <=128 MiB decoded.";
                else {
                    const int n = int(reader->lengthInSamples);
                    asset->left.resize(size_t(n)); asset->right.resize(size_t(n)); asset->rate = reader->sampleRate;
                    bool ok = true;
                    for (int start = 0; start < n && !threadShouldExit(); start += 65536) {
                        const int count = std::min(65536, n - start);
                        float* channels[] {asset->left.data() + start, asset->right.data() + start};
                        if (!reader->read(channels, 2, start, count)) { ok = false; break; }
                    }
                    if (threadShouldExit()) break;
                    if (!ok) { asset = std::make_unique<SampleData>(); result = "Sample decoding failed."; }
                    else {
                        if (reader->numChannels == 1) asset->right = asset->left;
                        for (size_t i = 0; i < asset->left.size(); ++i) {
                            auto clean = [](float x) { return std::isfinite(x) ? std::clamp(x, -8.0f, 8.0f) : 0.0f; };
                            asset->left[i] = clean(asset->left[i]); asset->right[i] = clean(asset->right[i]);
                        }
                        preview.resize(256, 0);
                        for (int bin = 0; bin < 256; ++bin) {
                            const size_t a = size_t(bin) * asset->left.size() / 256;
                            const size_t b = size_t(bin+1) * asset->left.size() / 256;
                            for (size_t i = a; i < b; ++i) preview[size_t(bin)] = std::max(preview[size_t(bin)], std::max(std::abs(asset->left[i]), std::abs(asset->right[i])));
                        }
                        result = juce::File(path).getFileName() + "  |  " + juce::String(double(n) / reader->sampleRate, 2) + " s  |  root MIDI 60 unless changed";
                    }
                }
            }
        } catch (const std::exception&) { asset = std::make_unique<SampleData>(); result = "Sample loading failed: memory or decoder error."; }
        {
            const juce::ScopedLock lock(mutex);
            handledId = id;
            if (requestId != id) continue; // A newer request won; never publish stale content.
            asset->generation = id;
            exchange.publish(std::move(asset));
            peaks = std::move(preview); message = result;
            completed.store(id); loading.store(false);
        }
    }
    exchange.collect();
}
}
