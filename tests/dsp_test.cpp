#include "../Source/DSP.h"
#include <cstdio>
#include <chrono>
using namespace smadder;
struct R { float peak=0, rms=0; bool bad=false; };
R run(Settings s, double sr, bool stereo, float amp, int seconds=3, bool silence=false){
  Engine e; e.prepare(sr); int n=int(sr*seconds), B=512; std::vector<float> L(B),Rt(B); R r; double acc=0; long cnt=0;
  for(int i=0;i<n;i+=B){ for(int j=0;j<B;j++){ double t=(i+j)/sr; float x= silence?0.f: amp*(0.6f*std::sin(2*kPi*110*t)+0.4f*std::sin(2*kPi*1320*t)); L[j]=x; Rt[j]=x*0.9f; }
    e.process(L.data(), stereo?Rt.data():nullptr, B, s);
    for(int j=0;j<B;j++){ for(float v: {L[j], stereo?Rt[j]:L[j]}){ if(!std::isfinite(v)) r.bad=true; r.peak=std::max(r.peak,std::fabs(v)); if(i>n/2){acc+=v*v;cnt++;} } } }
  r.rms = std::sqrt(acc/std::max(1L,cnt)); return r; }
void rep(const char* name, Settings s){
  for(double sr: {44100.0, 96000.0}) for(float amp: {0.1f, 0.9f}) for(bool st: {true,false}){
    R r=run(s,sr,st,amp); printf("%-18s sr=%6.0f amp=%.1f %s peak=%6.2f dB rms=%6.1f dB %s\n",name,sr,amp,st?"st":"mo",20*log10(r.peak+1e-9),20*log10(r.rms+1e-9), r.bad?"NAN!!":""); } }
int main(){
  Settings init; rep("init(bypass?)",init);
  Settings d=init; d.drive=1; rep("drive max",d);
  Settings c=init; c.bits=1; c.downsample=50; rep("crush max",c);
  Settings f=init; f.filterType=1; f.cutoff=200; f.reso=1; f.lfoDepth=1; f.lfoRate=20; rep("filter lp res max",f);
  f.filterType=3; f.cutoff=20000; rep("filter bp 20k",f);
  Settings w=init; w.wow=1; w.noise=1; rep("tape max",w);
  Settings dl=init; dl.delayMix=1; dl.feedback=0.95f; dl.delaySeconds=0.001f; dl.pingPong=true; dl.delayTone=18000; rep("delay fb max short",dl);
  dl.delaySeconds=4.0f; rep("delay 4s",dl);
  Settings all=init; all.drive=1; all.bits=1; all.downsample=50; all.filterType=1; all.cutoff=20; all.reso=1; all.lfoDepth=1; all.wow=1; all.noise=1; all.delayMix=1; all.feedback=0.95f; all.outputDb=12; rep("everything max",all);
  // silence out with noise 0 and delay tail -> should decay to 0
  Settings t=init; t.delayMix=1; t.feedback=0.95f; t.drive=0.5f; R r=run(t,44100,true,0.f,3,true); printf("silence in, no noise: peak=%g\n", r.peak);
  // bypass accuracy at init
  { Engine e; e.prepare(44100); float L[64],Rr[64]; for(int i=0;i<64;i++){L[i]=Rr[i]=0.5f*std::sin(i*0.1f);} float ref[64]; std::copy(L,L+64,ref); e.process(L,Rr,64,init); float md=0; for(int i=0;i<64;i++) md=std::max(md,std::fabs(L[i]-ref[i])); printf("init max deviation from dry: %g\n", md); }
  // CPU
  auto t0=std::chrono::steady_clock::now(); run(all,48000,true,0.5f,20); auto dt=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count(); printf("CPU: 20s stereo audio in %.3fs (%.1f%% of realtime)\n", dt, dt/20*100);
}
