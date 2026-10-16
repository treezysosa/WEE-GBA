#include "LoopExporter.h"
#include <stdexcept>
namespace wee {
void LoopExporter::run(){
    while(!threadShouldExit()){
        std::unique_ptr<LoopExportJob> job;uint64_t id=0;
        {const juce::ScopedLock lock(mutex);if(pending){job=std::move(pending);id=serial.load();}}
        if(!job){wait(30);continue;}
        auto cancelled=[&]{return threadShouldExit() || serial.load()!=id;};
        bool ok=false;juce::String error;
        try{
            if(!job->document.pattern.valid() || job->document.pattern.count==0)throw std::runtime_error("Generate a pattern first.");
            juce::TemporaryFile temporary(job->destination);
            if(job->midi){const auto bytes=patternMidi(job->document.pattern,int(std::lround(job->bpm)));ok=temporary.getFile().replaceWithData(bytes.data(),bytes.size());}
            else{
                auto rack=std::make_unique<LayerRack>();rack->prepare(48000);
                auto sample=std::make_unique<SampleData>();bool needsSample=false;
                for(const auto& l:job->layers)if(l.v[Enabled]>=.5f && (l.engine()==1 || l.engine()==4 || l.engine()==5))needsSample=true;
                if(needsSample){
                    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(job->samplePath)));
                    if(!reader || !std::isfinite(reader->sampleRate) || reader->sampleRate<8000 || reader->sampleRate>192000 || reader->numChannels<1 || reader->numChannels>2 || reader->lengthInSamples<2 || reader->lengthInSamples>16*1024*1024 || reader->lengthInSamples>reader->sampleRate*120)throw std::runtime_error("Cannot render: sample missing or unsupported.");
                    sample->rate=reader->sampleRate;const int n=int(reader->lengthInSamples);sample->left.resize(size_t(n));sample->right.resize(size_t(n));
                    for(int offset=0;offset<n && !cancelled();offset+=65536){float* channels[]{sample->left.data()+offset,sample->right.data()+offset};if(!reader->read(channels,2,offset,std::min(65536,n-offset)))throw std::runtime_error("Sample decode failed.");}
                    if(cancelled())continue;if(reader->numChannels==1)sample->right=sample->left;
                    for(size_t i=0;i<sample->left.size();++i){auto clean=[](float v){return std::isfinite(v)?std::clamp(v,-8.0f,8.0f):0;};sample->left[i]=clean(sample->left[i]);sample->right[i]=clean(sample->right[i]);}
                    sample->slices=job->slices;const float shift=job->vocal.v[VFormant]-(job->vocal.v[VPreserve]>=.5f?job->vocal.v[VPitch]:0);
                    if(!shiftFormants(sample->left,sample->vocalLeft,sample->rate,shift,cancelled) || !shiftFormants(sample->right,sample->vocalRight,sample->rate,shift,cancelled))continue;
                    rack->setSample(sample.get());
                }
                if(job->document.pattern.mode==3){bool vocal=false;for(const auto& l:job->layers)vocal|=l.v[Enabled]>=.5f && l.engine()==5;if(!vocal || !sample->valid())throw std::runtime_error("Vocal chops need an enabled vocal layer and recording.");}
                rack->configure(job->layers,job->master,job->bpm);rack->configureVocal(job->vocal);
                auto stream=temporary.getFile().createOutputStream();if(!stream)throw std::runtime_error("Cannot open export destination.");
                juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),48000,2,24,{},0));if(!writer)throw std::runtime_error("Cannot create WAV writer.");stream.release();
                PatternPlayer player;juce::AudioBuffer<float> buffer(2,512);const auto frames=int64_t(std::llround(job->document.pattern.beats()*60/job->bpm*48000));
                for(int64_t offset=0;offset<frames && !cancelled();offset+=512){const int count=int(std::min(int64_t(512),frames-offset));buffer.clear();player.begin(&job->document.pattern,true,count,48000,job->bpm,false,true,false,0);
                    player.until(count,[&](int from,int end){rack->render(buffer.getWritePointer(0,from),buffer.getWritePointer(1,from),end-from);},[&](const PatternEvent& e){if(job->document.pattern.mode==3){if(e.on)rack->sequenceSlice(e.note,e.velocity/127.0f,e.degree,true);else rack->sequenceOff(e.note);}else if(e.on)rack->noteOn(e.note,e.velocity/127.0f);else rack->noteOff(e.note);});
                    for(int i=0;i<count;++i){const float fade=float(std::clamp(double(std::min(offset+i,frames-1-offset-i))/240,0.0,1.0));for(int c=0;c<2;++c)buffer.setSample(c,i,buffer.getSample(c,i)*fade);}
                    if(!writer->writeFromAudioSampleBuffer(buffer,0,count))throw std::runtime_error("WAV write failed.");
                }
                writer.reset();ok=!cancelled();
            }
            if(ok && !cancelled())ok=temporary.overwriteTargetFileWithTemporary();else ok=false;
        }catch(const std::exception& e){error=e.what();}
        {const juce::ScopedLock lock(mutex);if(serial.load()==id){busy.store(false);message=ok?"Exported: "+job->destination.getFileName():"Export failed: "+(error.isEmpty()?juce::String("cancelled or write error"):error);}}
    }
}
}
