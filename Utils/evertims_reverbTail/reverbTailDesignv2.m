% Test reverb tail design for EVERTims Audio Engine
%
% same as v1 but based on gaussian noise sampling
% 
% David Poirier-Quinot, IRCAM 2016

% TODO:
% - assess CPU cost of this "full convolution based method". If too expensive, 
%   use the approach described in Jot's paper
% - when does the late reverb starts? at the end of early reflections? (~80ms), 
%   at the end of EVERTims source image array (no..)?, at -5dB? 

%% Init
RT60 = 0.8; % Response Time -60dB in sec
initGain = 1/12; % from Moorer 1979 "About this reverberation business"
initGainDb = 20*log10(initGain);
initDelay = 0.08; % in sec, arbitrary end of early reflextions
tailSlope = -60 / RT60; % dB

% load audio 
% load handel; % defines y, Fs
% duration = 0.001; % i n sec
% y = y(1:floor(duration*Fs),1);
[y,Fs] = audioread('VMH1 Claps & Snares 023.wav'); % clap
% [y,Fs] = audioread('VMH1 Claps & Snares 012.wav'); % snare
y = y(:,1);
y = [1; zeros(9, 1)];

%% Compute reverb tail

gaussianNoise = 2*(rand(ceil(Fs * RT60), 1) -0.5);
gain_f = @(initDelay, initGain, currentDelay) 10.^( ( 20*log10(initGain) + (currentDelay-initDelay) * tailSlope ) /20 ); % in secs
time_v = [0:1/Fs:(size(gaussianNoise,1)-1)/Fs].';
gain_v = gain_f(initDelay, initGain, time_v + initDelay + 1/Fs);
reverbTail = gain_v.*gaussianNoise;
% reverbTail = reverbTail(1);
subplot(221), plot(time_v, reverbTail); xlabel('time'); ylabel('ampl'); title('reverb tail');
subplot(222), semilogy(time_v, reverbTail); xlabel('time'); ylabel('ampl dB'); title('reverb tail');

%% apply on audio
subplot(223), plot(0:1/Fs:(size(y,1)-1)/Fs, y); xlabel('time'); ylabel('ampl'); title('audio in');
initDelayInSamples = ceil(initDelay * Fs);

% reverb tail convolution
out = conv(reverbTail, y);
% add direct sound
out = [zeros(Fs*initDelay, 1); out];
out( 1 : length(y), 1 ) = y; % direct sound

subplot(224), plot(0:1/Fs:(size(out,1)-1)/Fs, out); xlabel('time'); ylabel('ampl'); title('audio out');
a = audioplayer(out/max(abs(out)),Fs);
play(a);

%%
grain = 1;
out1 = zeros( initDelayInSamples + size(reverbTail,1) + length(y) - 1, 1);
out1( 1 : length(y), 1 ) = y; % direct sound
for i = 1:grain:size(reverbTail,1);
    
    index = i + initDelayInSamples;
    % out1( index : index+length(y)-1 ) = out1( index : index+length(y)-1 ) + 3 * rms(y) * reverbTail(i) * y;
    out1( index : index+length(y)-1 ) = out1( index : index+length(y)-1 ) + reverbTail(i) * y;
end
clf, subplot(311), plot(out), subplot(312), plot(out1), subplot(313), plot(out-out1);

%% C / C++ convolution
% nconv = size(reverbTail, 1) + length(y) - 1;
% out2 = zeros(nconv, 1);
% 
% for i = 1:nconv;
%     i1 = i;
%     tmp = 0;
%     for j = 1:length(y)
%         if( (i1 > 0) && (i1 <= size(reverbTail, 1)) )
%             tmp = tmp + reverbTail(i1) * y(j);
%         else break
%         end
%         i1 = i1 - 1;
%     end
%     out2(i) = tmp;
% end

% //convolution algorithm
% float *conv(float *A, float *B, int lenA, int lenB, int *lenC)
% {
% 	int nconv;
% 	int i, j, i1;
% 	float tmp;
% 	float *C;
%  
% 	//allocated convolution array	
% 	nconv = lenA+lenB-1;
% 	C = (float*) calloc(nconv, sizeof(float));
%  
% 	//convolution process
% 	for (i=0; i<nconv; i++)
% 	{
% 		i1 = i;
% 		tmp = 0.0;
% 		for (j=0; j<lenB; j++)
% 		{
% 			if(i1>=0 && i1<lenA)
% 				tmp = tmp + (A[i1]*B[j]);
%  
% 			i1 = i1-1;
% 			C[i] = tmp;
% 		}
% 	}
%  
% 	//get length of convolution array	
% 	(*lenC) = nconv;
%  
% 	//return convolution array
% 	return(C);
% }