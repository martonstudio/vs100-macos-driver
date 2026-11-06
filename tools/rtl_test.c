/*
 * rtl_test — measures the real analog round-trip latency of the VS-100 through
 * CoreAudio: connect a cable from the VS-100 output to its input, then run
 *   rtl_test [bufferFrames]
 * It emits a short click every 500 ms and finds it in the input stream.
 */
#include <CoreAudio/CoreAudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
static float *rec; static unsigned long recN = 0, recCap; static double rate = 48000;
static unsigned long clickPos[64]; static UInt32 recCh = 0; static int nClicks = 0;
static OSStatus proc(AudioObjectID dev,const AudioTimeStamp*now,const AudioBufferList*in,const AudioTimeStamp*inT,AudioBufferList*out,const AudioTimeStamp*outT,void*c){
  (void)dev;(void)now;(void)inT;(void)outT;(void)c;
  /* record input (channel 0) indexed by input sample time */
  for(UInt32 b=0;b<in->mNumberBuffers && b<1;b++){ const float*p=in->mBuffers[b].mData; UInt32 ch=in->mBuffers[b].mNumberChannels; UInt32 n=in->mBuffers[b].mDataByteSize/4/ch; unsigned long start=(unsigned long)inT->mSampleTime; for(UInt32 i=0;i<n;i++){ unsigned long idx=start+i; if(idx<recCap){ rec[idx]=p[i*ch+(recCh<ch?recCh:0)]; if(idx+1>recN) recN=idx+1; } } }
  for(UInt32 b=0;b<out->mNumberBuffers;b++){ float*p=out->mBuffers[b].mData; UInt32 ch=out->mBuffers[b].mNumberChannels; UInt32 n=out->mBuffers[b].mDataByteSize/4/ch; unsigned long start=(unsigned long)outT->mSampleTime;
    for(UInt32 i=0;i<n;i++){ unsigned long idx=start+i; float v=0; unsigned long ph=idx%((unsigned long)(rate/2)); if(ph<24) { v=0.5f*(ph<12?1.f:-1.f); if(ph==0 && nClicks<64) clickPos[nClicks++]=idx; } for(UInt32 k=0;k<ch;k++) p[i*ch+k]=v; } }
  return 0; }
int main(int argc,char**argv){ UInt32 bs = argc>1?atoi(argv[1]):64; recCh = argc>2?(UInt32)atoi(argv[2])-1:0;   /* [bufferFrames] [input channel, 1-based] */
  AudioObjectPropertyAddress pa={kAudioHardwarePropertyDevices,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain}; UInt32 sz=0; AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,&pa,0,NULL,&sz); AudioObjectID ids[64]; AudioObjectGetPropertyData(kAudioObjectSystemObject,&pa,0,NULL,&sz,ids); AudioObjectID dev=0;
  for(UInt32 i=0;i<sz/4;i++){ CFStringRef n; UInt32 s=sizeof n; AudioObjectPropertyAddress na={kAudioDevicePropertyModelUID,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain}; if(AudioObjectGetPropertyData(ids[i],&na,0,NULL,&s,&n)==0){ char b[128]; CFStringGetCString(n,b,128,kCFStringEncodingUTF8); if(strstr(b,"VS-100")) dev=ids[i]; } }
  if(!dev){printf("VS-100 not found\n");return 1;}
  UInt32 s=8; AudioObjectPropertyAddress ra={kAudioDevicePropertyNominalSampleRate,kAudioObjectPropertyScopeGlobal,0}; AudioObjectGetPropertyData(dev,&ra,0,NULL,&s,&rate);
  UInt32 safI,safO; s=4; AudioObjectPropertyAddress sa={kAudioDevicePropertySafetyOffset,kAudioObjectPropertyScopeInput,0}; AudioObjectGetPropertyData(dev,&sa,0,NULL,&s,&safI); sa.mScope=kAudioObjectPropertyScopeOutput; AudioObjectGetPropertyData(dev,&sa,0,NULL,&s,&safO);
  AudioObjectPropertyAddress ba={kAudioDevicePropertyBufferFrameSize,kAudioObjectPropertyScopeGlobal,0}; AudioObjectSetPropertyData(dev,&ba,0,NULL,4,&bs); s=4; AudioObjectGetPropertyData(dev,&ba,0,NULL,&s,&bs);
  printf("rate=%.0f buffer=%u safety in=%u out=%u  (driver-side theoretical: %.2f ms + 2x buffer %.2f ms)\n",rate,bs,safI,safO,(safI+safO)*1000.0/rate,2*bs*1000.0/rate);
  recCap=(unsigned long)(rate*8); rec=calloc(recCap,sizeof(float));
  AudioDeviceIOProcID id; AudioDeviceCreateIOProcID(dev,proc,NULL,&id); AudioDeviceStart(dev,id); sleep(5); AudioDeviceStop(dev,id); AudioDeviceDestroyIOProcID(dev,id);
  double peakAll=0; for(unsigned long i=0;i<recN;i++) if(fabs(rec[i])>peakAll) peakAll=fabs(rec[i]);
  printf("input peak %.3f (%.1f dBFS), %d clicks emitted\n",peakAll,peakAll>0?20*log10(peakAll):-200.0,nClicks);
  if(peakAll<0.01){ printf("No signal on the input: is the loopback cable connected (output -> input) and input gain up?\n"); return 1; }
  /* onset = first sample whose |value| crosses 25% of the overall peak, linearly interpolated.
     (Peak-picking is wrong here: the converter filters ring and the maximum wanders by +-20 samples.) */
  int found=0; double sum=0, mn=1e9, mx=0;
  for(int c=1;c<nClicks;c++){ unsigned long from=clickPos[c], to=clickPos[c]+(unsigned long)(rate*0.2); if(to>recN) break; double thr=0.25*peakAll; unsigned long i=from; while(i<to && fabs(rec[i])<thr) i++; if(i>=to||i==from) continue;
    double a=fabs(rec[i-1]), b=fabs(rec[i]); double d=(double)(i-1-from)+(thr-a)/(b-a); if(getenv("RTL_VERBOSE")) printf("click %d: onset +%.2f samples\n",c,d); found++; sum+=d; if(d<mn)mn=d; if(d>mx)mx=d; }
  if(!found){ printf("clicks not detected in input\n"); return 1; }
  printf("round-trip: mean %.1f samples = %.2f ms (min %.2f, max %.2f ms) over %d clicks; jitter (max-min) %.2f samples\n", sum/found, sum/found*1000/rate, mn*1000/rate, mx*1000/rate, found, mx-mn);
  printf("(includes ADC/DAC converter latency of the VS-100 itself; jitter between clicks should be < 1 sample)\n");
  return 0; }
