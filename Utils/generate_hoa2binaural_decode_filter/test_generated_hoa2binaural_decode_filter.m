% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %
% 
%     This script is part of the EVERTims Sound Engine framework
% 
%     Test generated HOA to Binaural decoding filters
% 
%     Author: David Poirier-Quinot
%     IRCAM, 2017
% 
% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %

%% INIT

% set current folder to script's location
cd(fileparts(mfilename('fullpath'))); 

% add subfolders of current dir in search path
root_path = pwd;
addpath(genpath(root_path));

% define paths
input_path = fullfile(root_path,'output');
% filename = 'hoa2bin_order2_ClubFritz1.bin'; nSample_bin = 1024;
% filename = 'Set1_without_direct_sound_abir.bin'; nSample_bin = 11140;
filename = 'hoa2bin_order2_IRC_1008_R_HRIR.bin'; nSample_bin = 221;
filepath = fullfile(input_path, filename);

%% Load HOA2bin filters
ambisonicOrder = 2;
numSamplesExpected = 221;
ambi2binIr = loadBinAmbi2Bin(filepath, ambisonicOrder, numSamplesExpected);

% plot data
y_max = max(max(abs([ ambi2binIr.l_m ; ambi2binIr.r_m ])));
nSample_toplot = numSamplesExpected;
nAmbiChannels = size(ambi2binIr.l_m,1);
for i = 1:nAmbiChannels
    index_ir = i;
    subplot(nAmbiChannels,2,1 + 2*(i-1)),plot(ambi2binIr.l_m(index_ir,1:nSample_toplot)), title(sprintf('Left, Ambi channel: %ld, RMS=%0.5f', i, rms(ambi2binIr.l_m(index_ir,:)))); xlabel('time'), ylabel('amp');
    axis([1 nSample_toplot -y_max y_max]);
    subplot(nAmbiChannels,2, 2*i),plot(ambi2binIr.r_m(index_ir,1:nSample_toplot)), title(sprintf('Right, Ambi channel: %ld, RMS=%0.5f', i, rms(ambi2binIr.r_m(index_ir,:)))); xlabel('time'), ylabel('amp');
    axis([1 nSample_toplot -y_max y_max]);
%     if i < nAmbiChannels; input(''); end;
end;

%% Check: create "ambisonic source" (source + ambisonic encode)

% load audio file
load handel; audioIn = y; clear y;

% Ambisonic encode
src_dir = [90 0]; % [azim elev]
hoasig = encodeHOA_N3D(ambisonicOrder, audioIn, src_dir); % WYZX

%% Check: decode with loaded HOA2BIN filters

% convole
MAXRE_ON = 1; % Max-rE weighting On/Off, per-order weighting of the decoder, resulting in maximum-norm energy vectors
decoder = 'allrad'; % ALL-ROUND AMBISONIC DECODING

clear left, clear right,
for i = 1: size(hoasig,2)
    left(:,i) = conv(ambi2binIr.l_m(i, :), hoasig(:, i)');
    right(:,i) = conv(ambi2binIr.r_m(i, :), hoasig(:, i)');
end

% sum outputs
leftFinal = sum(left, 2);
rightFinal = sum(right, 2);

duration = 2; % in sec
audioOut = [leftFinal(1:duration*Fs), rightFinal(1:duration*Fs)];
audioOut = audioOut / max(max(abs(audioOut)));

%% Check: play output

player = audioplayer(audioOut, Fs);
play(player)
