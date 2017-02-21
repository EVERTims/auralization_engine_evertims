% Test reverb tail design for EVERTims Audio Engine
%
% David Poirier-Quinot, IRCAM 2016

%% Init
BUFFER_LENGTH = 512;
Fs = 44100;
MIN_DELAY_BETWEEN_TAIL_TAP = 0.001;
MAX_DELAY_BETWEEN_TAIL_TAP = 0.007;

% values for shova mat
RT60 = 0.28; % Response Time -60dB in sec % 	[ 0.39, 0.39, 0.39, 0.35, 0.28, 0.22, 0.26, 0.33, 0.25, 0.25 ]
initGain = 0.0199;
initGainDb = 20*log10(initGain);
initDelay = 0.146; % in sec, arbitrary end of early reflextions

% values for mirror mat
RT60 = 0.43; % Response Time -60dB in sec % [ 0.36, 0.36, 0.36, 0.36, 0.36, 0.36, 0.36, 0.36, 0.36, 0.36 ]
initGain = 0.02;
initGainDb = 20*log10(initGain);
initDelay = 0.146; % in sec, arbitrary end of early reflextions
reverbTail = [initDelay, initGain];
% tailSlope = initGain * (10^-6 - 1) / RT60;
tailSlope = -60 / RT60; % dB

%% Compute reverb tail
timeIncr = @(minDelay, maxDelay) minDelay + rand() * (maxDelay - minDelay);

minDelay = MIN_DELAY_BETWEEN_TAIL_TAP;
maxDelay = MAX_DELAY_BETWEEN_TAIL_TAP;
dynamicTailSizeGain = RT60;
time = initDelay + timeIncr( minDelay, maxDelay ) * dynamicTailSizeGain;
while( time < (initDelay+RT60) );
    gainDb = (time-initDelay) * tailSlope;
    gain = initGain*10^(gainDb/20);
    reverbTail = [ reverbTail; [time, gain]];
    time = time + timeIncr(minDelay, maxDelay) * dynamicTailSizeGain;
end

stem(reverbTail(:,1), reverbTail(:,2));
ylabel('ampl (s.i.)'); xlabel('time (sec)'); title('reverb tail');
sum(reverbTail(:,2))

fprintf('num tap in reverb tail: %ld \n', size(reverbTail, 1));
fprintf('(max: %ld, min: %ld) \n',...
    RT60 / (MAX_DELAY_BETWEEN_TAIL_TAP * dynamicTailSizeGain), ...
    RT60 / (MIN_DELAY_BETWEEN_TAIL_TAP * dynamicTailSizeGain) );
fprintf('num buffer worth of tail: %.1f \n', Fs * RT60 / BUFFER_LENGTH);
fprintf('tail tap density per second: %.1f \n', size(reverbTail, 1) / RT60);
fprintf('tail tap density buffer-wise: %.1f \n', size(reverbTail, 1) / (Fs * RT60 / BUFFER_LENGTH) );

%% apply on audio
load handel;
duration = 1; % in sec
y = y(1:duration*Fs,1);
d = 1; y = zeros(Fs,1); y(1:d) = ones(d,1); % dirach
% a = audioplayer(y,Fs);
% playblocking(a);

out = zeros( ceil(max(reverbTail(:,1)) * Fs) + length(y), 1);
for i = 1:size(reverbTail,1);
    indexStart = floor(reverbTail(i,1) * Fs);
    out( indexStart : indexStart+length(y)-1 ) = out( indexStart : indexStart+length(y)-1 ) + reverbTail(i,2) .* y;
end

a = audioplayer(out/max(abs(out)),Fs);
playblocking(a);
subplot(211), plot(0:1/Fs:(size(y,1)-1)/Fs, y);
subplot(212), plot(0:1/Fs:(size(out,1)-1)/Fs, out);