#include "SampleLoader.h"
namespace wee {
namespace {
bool writeProcessed(const SampleData& raw,const SliceMap& slices,const juce::File& file,const VocalConfig& settings,double bpm,int slice,const std::function<bool()>& cancelled){
    auto asset=std::make_unique<SampleData>(raw);asset->slices=slices;
    const float shift=settings.v[VFormant]-(settings.v[VPreserve]>=.5f?settings.v[VPitch]:0);
    if(!shiftFormants(asset->left,asset->vocalLeft,asset->rate,shift,cancelled) || !shiftFormants(asset->right,asset->vocalRight,asset->rate,shift,cancelled))return false;
    auto tables=std::make_unique<WavetableBank>();tables->prepare();VocalEngine engine;engine.prepare(asset->rate,*tables);engine.setSample(asset.get());
    auto c=settings;c.v[VChopMap]=0;LayerConfig layer;layer.v[Root]=60;layer.v[Loop]=0;
    Parameters p;p.gain=1;p.attack=.001f;p.sustain=1;p.bypassTone=true;
    if(slice>=slices.count || slice<-1)return false;
    engine.noteOn(60,1,layer,c,2,slice);
    const double proportion=slice>=0?slices.points[size_t(slice+1)]-slices.points[size_t(slice)]:1;
    const int64_t frames=std::max(int64_t(1),int64_t(std::ceil(double(asset->left.size())*proportion*c.stretch(bpm))));
    juce::TemporaryFile temporary(file);auto stream=temporary.getFile().createOutputStream();if(!stream)return false;
    juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),asset->rate,2,24,{},0));if(!writer)return false;stream.release();
    juce::AudioBuffer<float> buffer(2,512);
    for(int64_t offset=0;offset<frames;offset+=512){
        if(cancelled())return false;const int count=int(std::min(int64_t(512),frames-offset));buffer.clear();
        engine.render(buffer.getWritePointer(0),buffer.getWritePointer(1),count,p,c,bpm,0,true);
        for(int channel=0;channel<2;++channel)for(int i=0;i<count;++i)buffer.setSample(channel,i,std::clamp(buffer.getSample(channel,i),-1.0f,1.0f));
        if(!writer->writeFromAudioSampleBuffer(buffer,0,count))return false;
    }
    writer.reset();return temporary.overwriteTargetFileWithTemporary();
}
}
void SampleLoader::run(){
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<SampleData> raw;juce::String cachePath;juce::int64 cacheSize=-1;juce::Time cacheTime;uint64_t sourceSerial=0,publishSerial=0;
    while(!threadShouldExit()){
        exchange.collect();juce::String path;SliceDocument document;float sensitivity=.5f;uint64_t id=0,revision=shiftRevision.load();float shift=targetShift.load();bool force=false,prepare=false;
        {
            const juce::ScopedLock lock(mutex);
            prepare=requestId!=handledId || revision!=handledShift.load();
            if(prepare){id=requestId;path=requestedPath;document=requestedDocument;sensitivity=requestedSensitivity;force=requestId!=handledId && forceDecode;loading.store(true);}
        }
        if(prepare){
            auto cancelled=[&]{return threadShouldExit() || requestSignal.load()!=id || shiftRevision.load()!=revision;};
            // Wait for a stable formant target instead of copying large assets on
            // every automation tick. File/marker requests still start immediately.
            if(id==handledId && revision!=handledShift.load()){wait(60);if(cancelled())continue;}
            auto asset=std::make_unique<SampleData>();std::vector<float> preview;juce::String result="No sample loaded";
            try{
                if(path.isEmpty()){raw.reset();cachePath.clear();}
                else{
                    const juce::File file(path);const auto modified=file.getLastModificationTime();const auto size=file.getSize();
                    if(force || !raw || cachePath!=path || cacheTime!=modified || cacheSize!=size){
                        raw.reset();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                        if(!reader)result="Cannot read sample. Use LOAD SAMPLE / RELOCATE for WAV, AIFF or FLAC.";
                        else if(!std::isfinite(reader->sampleRate) || reader->lengthInSamples<2 || reader->sampleRate<8000 || reader->sampleRate>192000 || reader->numChannels<1 || reader->numChannels>2 || reader->lengthInSamples>juce::int64(reader->sampleRate*120) || reader->lengthInSamples>16*1024*1024)
                            result="Unsupported sample: mono/stereo, 8–192 kHz, <=120 s, <=128 MiB original audio.";
                        else{
                            auto decoded=std::make_unique<SampleData>();const int n=int(reader->lengthInSamples);decoded->left.resize(size_t(n));decoded->right.resize(size_t(n));decoded->rate=reader->sampleRate;bool ok=true;
                            for(int start=0;start<n && !cancelled();start+=65536){const int count=std::min(65536,n-start);float* channels[]{decoded->left.data()+start,decoded->right.data()+start};if(!reader->read(channels,2,start,count)){ok=false;break;}}
                            if(cancelled())continue;
                            if(!ok)result="Sample decoding failed.";
                            else{if(reader->numChannels==1)decoded->right=decoded->left;for(size_t i=0;i<decoded->left.size();++i){auto clean=[](float x){return std::isfinite(x)?std::clamp(x,-8.0f,8.0f):0;};decoded->left[i]=clean(decoded->left[i]);decoded->right[i]=clean(decoded->right[i]);}decoded->sourceGeneration=++sourceSerial;raw=std::move(decoded);cachePath=path;cacheTime=modified;cacheSize=size;}
                        }
                    }
                    if(raw){
                        *asset=*raw;asset->slices=document.manual?document.map:detectSlices(*asset,sensitivity);
                        if(!shiftFormants(asset->left,asset->vocalLeft,asset->rate,shift,cancelled) || !shiftFormants(asset->right,asset->vocalRight,asset->rate,shift,cancelled))continue;
                        preview.resize(256,0);for(int bin=0;bin<256;++bin){const size_t a=size_t(bin)*asset->left.size()/256,b=size_t(bin+1)*asset->left.size()/256;for(size_t i=a;i<b;++i)preview[size_t(bin)]=std::max(preview[size_t(bin)],std::max(std::abs(asset->left[i]),std::abs(asset->right[i])));}
                        result=file.getFileName()+" | "+juce::String(double(asset->left.size())/asset->rate,2)+" s | "+juce::String(asset->slices.count)+" chops | formant "+juce::String(shift,1)+" st";
                    }
                }
            }catch(const std::exception&){asset=std::make_unique<SampleData>();result="Sample/vocal preparation failed: memory or decoder error.";}
            {
                const juce::ScopedLock lock(mutex);if(cancelled())continue;
                handledId=id;handledShift.store(revision);asset->generation=++publishSerial;previewSlices=asset->slices;exchange.publish(std::move(asset));peaks=std::move(preview);message=result;completed.store(publishSerial);loading.store(false);sourceLoading.store(false);
            }
            continue;
        }
        juce::File destination;VocalConfig settings;double bpm=120;int slice=-1;uint64_t eid=0,source=0;
        {
            const juce::ScopedLock lock(mutex);if(exportId!=handledExport){eid=exportId;destination=exportPath;settings=exportConfig;bpm=exportBpm;slice=exportSlice;source=exportSource;}
        }
        if(eid){
            bool success=false;try{if(raw && requestSignal.load()==source)success=writeProcessed(*raw,previewSlices,destination,settings,bpm,slice,[&]{return threadShouldExit() || requestSignal.load()!=source;});}catch(const std::exception&){}
            const juce::ScopedLock lock(mutex);handledExport=eid;if(exportId==eid){exporting.store(false);exportMessage=success?"WAV exported: "+destination.getFileName():"WAV export failed/cancelled. Check loaded content, slice and destination.";if(success)lastExportFile=destination;}
        }else wait(20);
    }
    exchange.collect();
}
}
