#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include "Pattern.h"
#include <charconv>
namespace wee {
inline juce::ValueTree generatorTree(const GeneratorDocument& doc){
    juce::ValueTree t("GENERATOR");const auto& c=doc.settings;
    auto put=[&](const char* id,int v){t.setProperty(id,v,nullptr);};
    put("version",1);put("seed",int(c.seed));put("style",c.style);put("mode",c.mode);put("bars",c.bars);put("root",c.root);put("scale",c.scale);put("octave",c.octave);
    put("complexity",c.complexity);put("humanize",c.humanize);put("swing",c.swing);put("density",c.density);put("variation",c.variation);put("tempo",c.tempo);
    put("lockMelody",c.lockMelody);put("lockRhythm",c.lockRhythm);put("lockKey",c.lockKey);put("lockSound",c.lockSound);put("hostSync",c.hostSync);
    put("patternBars",doc.pattern.bars);put("patternMode",doc.pattern.mode);
    for(int i=0;i<16;++i){juce::ValueTree chord("CHORD");chord.setProperty("degree",doc.pattern.chords[size_t(i)],nullptr);t.appendChild(chord,nullptr);}
    for(int i=0;i<doc.pattern.count;++i){const auto& n=doc.pattern.notes[size_t(i)];juce::ValueTree note("NOTE");note.setProperty("beat",n.beat,nullptr);note.setProperty("duration",n.duration,nullptr);note.setProperty("pitch",n.note,nullptr);note.setProperty("velocity",n.velocity,nullptr);note.setProperty("degree",n.degree,nullptr);t.appendChild(note,nullptr);}return t;
}
inline bool readGenerator(const juce::ValueTree& t,GeneratorDocument& doc){
    if(!t.hasType("GENERATOR") || t.getNumChildren()<16 || t.getNumChildren()>maxPatternNotes+16)return false;
    auto integer=[&](const juce::ValueTree& node,const char* id,int& value){const auto s=node.getProperty(id).toString().toStdString();const auto r=std::from_chars(s.data(),s.data()+s.size(),value);return r.ec==std::errc{} && r.ptr==s.data()+s.size();};
    auto get=[&](const char* id,int& value){return integer(t,id,value);};
    auto flag=[&](const char* id,bool& value){int n=0;if(!get(id,n) || n<0 || n>1)return false;value=n!=0;return true;};
    auto real=[&](const juce::ValueTree& node,const char* id,double& value){const auto s=node.getProperty(id).toString().toStdString();const auto r=std::from_chars(s.data(),s.data()+s.size(),value);return r.ec==std::errc{} && r.ptr==s.data()+s.size() && std::isfinite(value);};
    auto& c=doc.settings;auto& p=doc.pattern;int version=0,seed=0;
    if(!get("version",version) || version!=1 || !get("seed",seed) || seed<0 || !get("style",c.style) || !get("mode",c.mode) || !get("bars",c.bars) || !get("root",c.root) || !get("scale",c.scale) || !get("octave",c.octave) || !get("complexity",c.complexity) || !get("humanize",c.humanize) || !get("swing",c.swing) || !get("density",c.density) || !get("variation",c.variation) || !get("tempo",c.tempo) || !flag("lockMelody",c.lockMelody) || !flag("lockRhythm",c.lockRhythm) || !flag("lockKey",c.lockKey) || !flag("lockSound",c.lockSound) || !flag("hostSync",c.hostSync) || !get("patternBars",p.bars) || !get("patternMode",p.mode))return false;
    c.seed=uint32_t(seed);if(!c.valid())return false;p.count=t.getNumChildren()-16;
    for(int i=0;i<16;++i){const auto child=t.getChild(i);if(!child.hasType("CHORD") || !integer(child,"degree",p.chords[size_t(i)]))return false;}
    for(int i=0;i<p.count;++i){const auto child=t.getChild(i+16);auto& n=p.notes[size_t(i)];if(!child.hasType("NOTE") || !real(child,"beat",n.beat) || !real(child,"duration",n.duration) || !integer(child,"pitch",n.note) || !integer(child,"velocity",n.velocity) || !integer(child,"degree",n.degree))return false;}
    if(!p.valid())return false;p.prepare();return true;
}
}
