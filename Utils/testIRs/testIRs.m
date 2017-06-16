% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %
% 
%     This script is part of the EVERTims Sound Engine framework
% 
%     load, plot and compare exported Ambisonic and Binaural IRs
% 
%     Author: David Poirier-Quinot
%     IRCAM, 2017
% 
% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %

%% check Binaural IR
% define input IR path
[binIr, Fs] = audioread('Evertims_IR_Recording_binaural.wav');

% plot IR
maxValue = max(max(abs(binIr)));
t = 0:1/Fs:((size(binIr,1)-1)/Fs);
for i = 1:size(binIr,2)
    subplot(2, 3, 1), plot(t, binIr(:,i) - (i-1)*2*maxValue); hold on
end
hold off
title(sprintf('Binaural IR channels from 1 to %ld (top to bottom)', i));
xlabel('time (sec)'); ylabel('amplitude S.I.'); legend({'left', 'right'});

% plot IR (logarithmic mono)
t = 0:1/Fs:((size(binIr,1)-1)/Fs);
subplot(2, 3, 4), plot(t, 20*log10(abs(binIr(:,1))) );
title('Binaural IR, logarithmic scale (left IR only)');
xlabel('time (sec)'); ylabel('amplitude 20*log10(S.I.)'); legend({'left', 'right'});


%% check Ambisonic IR
% define input IR path
[ambiIr, Fs] = audioread('Evertims_IR_Recording_ambi_2_order.wav');

% plot IR
maxValue = max(max(abs(ambiIr)));
t = 0:1/Fs:((size(ambiIr,1)-1)/Fs);
for i = 1:size(ambiIr,2)
    subplot(2, 3, [2, 5]), plot(t, ambiIr(:,i) - (i-1)*1.5*maxValue); hold on
end
hold off
title(sprintf('Ambisonic IR channels from 1 to %ld (top to bottom)', i));
xlabel('time (sec)'); ylabel('amplitude S.I.');

%% check (plot diff) Ambi IR compared to binaural IR
% load ambi2bin decode filters
filepath = fullfile(fileparts(pwd), 'generate_hoa2binaural_decode_filter', 'output', 'hoa2bin_order2_IRC_1008_R_HRIR.mat');
load(filepath, 'ambi2binIr');

% ambi 2 bin decoding of IR
binIr_synth = zeros( length(ambiIr) + length(ambi2binIr.l_m) - 1, 2);
for i = 1:size(ambiIr,2)
    binIr_synth(:,1) = binIr_synth(:,1) + conv(ambi2binIr.l_m(:, i), ambiIr(:, i));
    binIr_synth(:,2) = binIr_synth(:,2) + conv(ambi2binIr.r_m(:, i), ambiIr(:, i));
end

% plot diff. between binaural IRs synth from Ambisonic IRs and binaural IR
binIr_synth = binIr_synth(1:length(binIr), :);
binIr_delta = binIr_synth - binIr;
maxValue = max(max(abs(binIr_delta)));
for i = 1:size(binIr_delta,2);
    subplot(2, 3, [3, 6]), plot(t, binIr_delta - (i-1)*2*maxValue); hold on
end
hold off
title(sprintf('Diff between exported IRs: Binaural IR vs Binaural IR synth from Ambisonic IR'));
xlabel('time (sec)'); ylabel('amplitude S.I.'); legend({'left', 'right'});


%% check (listening test) Ambi IR compared to binaural IR
% generate impulse
y = [1 0 0 0];

% convolve (binaural encoding): rec. bin ir
outBin = [];
for i = 1:2; outBin(:,i) = conv(binIr(:,i), y); end
% playback
a = audioplayer(outBin,Fs);
playblocking(a);

% convolve (binaural encoding): synth (from Ambisonic) bin IR
outBinSynth = [];
binIr_synth = max(max(abs(binIr))) * binIr_synth / max(max(abs(binIr_synth)));
for i = 1:2; outBinSynth(:,i) = conv(binIr_synth(:,i), y); end
% playback
a = audioplayer(outBinSynth,Fs);
playblocking(a);
