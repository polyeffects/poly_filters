declare name "oberheimBSF";
declare description "Oberheim generic multi-outputs Filter";
declare author "Eric Tarr, GRAME";

import("stdfaust.lib");

Q = hslider("Q",1,0.5,10,0.01);
normFreq = hslider("freq",0.5,0,1,0.001):si.smoo;

// The BSF, BPF, HPF and LPF outputs are produced
process = _ : ve.oberheim(normFreq,Q);
