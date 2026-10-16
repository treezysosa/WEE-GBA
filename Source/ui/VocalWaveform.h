#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "vocal/SliceMap.h"
class VocalWaveform final:public juce::Component {
public:
    std::vector<float> peaks;wee::SliceMap markers;int selected=0;
    std::function<void(const wee::SliceMap&)> edited;
    std::function<void(int)> selection;
    std::function<void()> preview;
    VocalWaveform(){setWantsKeyboardFocus(true);setName("Editable vocal slice waveform");setTitle("Double click to split; drag marker; right click or Delete to remove; arrows select; Space previews");}
    void paint(juce::Graphics& g)override{
        g.fillAll(juce::Colour(0xff111e1a));const float w=float(getWidth()),h=float(getHeight());selected=juce::jlimit(0,markers.count-1,selected);
        g.setColour(juce::Colour(0xff9bdcad).withAlpha(.14f));g.fillRect(float(markers.points[size_t(selected)])*w,0.0f,float(markers.points[size_t(selected+1)]-markers.points[size_t(selected)])*w,h);
        g.setColour(juce::Colour(0xff9bdcad));for(size_t i=0;i<peaks.size();++i){float y=std::min(1.0f,peaks[i])*(h*.35f),x=float(i)/float(peaks.size())*w;g.drawVerticalLine(int(x),h*.55f-y,h*.55f+y);}
        g.setFont(11);for(int i=0;i<=markers.count;++i){float x=float(markers.points[size_t(i)])*w;g.setColour(i==selected?juce::Colours::white:juce::Colour(0xffffcf83));g.drawVerticalLine(juce::jlimit(0,getWidth()-1,int(x)),0,h);if(i<markers.count)g.drawText(juce::String(i+1),int(x)+3,2,28,14,juce::Justification::centredLeft);}
    }
    void select(int index){selected=juce::jlimit(0,markers.count-1,index);if(selection)selection(selected);repaint();}
    void split(){auto map=markers;if(map.add((map.points[size_t(selected)]+map.points[size_t(selected+1)])*.5)){markers=map;if(edited)edited(markers);repaint();}}
    void remove(){auto map=markers;if(map.remove(selected>0?selected:1)){markers=map;selected=std::min(selected,map.count-1);if(edited)edited(map);select(selected);}}
    void mouseDown(const juce::MouseEvent& e)override{
        grabKeyboardFocus();dragged=-1;const double p=position(e.x);select(markers.sliceAt(p));
        for(int i=1;i<markers.count;++i)if(std::abs(markers.points[size_t(i)]*getWidth()-e.x)<7){dragged=i;break;}
        if(e.mods.isRightButtonDown() && dragged>0){auto map=markers;map.remove(dragged);markers=map;dragged=-1;selected=std::min(selected,map.count-1);if(edited)edited(map);repaint();}
    }
    void mouseDrag(const juce::MouseEvent& e)override{if(dragged>0){markers.points[size_t(dragged)]=std::clamp(position(e.x),markers.points[size_t(dragged-1)]+.000001,markers.points[size_t(dragged+1)]-.000001);repaint();}}
    void mouseUp(const juce::MouseEvent&)override{if(dragged>0 && edited)edited(markers);dragged=-1;}
    void mouseDoubleClick(const juce::MouseEvent& e)override{auto map=markers;if(map.add(position(e.x))){markers=map;select(map.sliceAt(position(e.x)));if(edited)edited(map);}}
    bool keyPressed(const juce::KeyPress& key)override{
        if(key==juce::KeyPress::leftKey){select(selected-1);return true;}if(key==juce::KeyPress::rightKey){select(selected+1);return true;}
        if(key==juce::KeyPress::deleteKey || key==juce::KeyPress::backspaceKey){remove();return true;}if(key==juce::KeyPress::spaceKey){if(preview)preview();return true;}return false;
    }
private:
    int dragged=-1;double position(int x)const{return std::clamp(double(x)/std::max(1,getWidth()),0.0,1.0);}
};
