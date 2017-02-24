%     This script is part of the EVERTims Sound Engine framework
% 
%     Test generated HOA to Binaural decoding filters
% 
% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %

%% INIT

% set current folder to script's location
cd(fileparts(mfilename('fullpath'))); 

% add subfolders of current dir in search path
root_path = pwd;
addpath(genpath(root_path));

% define paths
input_path = fullfile(root_path,'output','hoa2binIRs');
% filename = 'hoa2bin_order2_ClubFritz1.bin'; nSample_bin = 1024;
% filename = 'Set1_without_direct_sound_abir.bin'; nSample_bin = 11140;
filename = 'hoa2bin_order2_IRC_1008_R_HRIR.bin'; nSample_bin = 161;

%% Load HOA2bin filters

% define filter parameters
ambisonicOrder = 2;

% Load data
fileID = fopen(fullfile(input_path, filename));
A = fread(fileID,'float32');
fclose(fileID);

% check data
nAmbiChannels = (ambisonicOrder+1)^2;
nChannels = 2; % L / R
nSample_bin_real = size(A,1) / ( nAmbiChannels * nChannels );
if (nSample_bin_real ~= nSample_bin);
    error('announced and real number of samples differ \n announced: %ld \n real: %ld \n', nSample_bin, nSample_bin_real );
end

% shape data 
AA = reshape(A,nSample_bin,[]).';
A_L = []; A_R = [];
for i = 1:size(AA,1)/2;
    A_L = [A_L; AA(2*(i-1)+1,:)];
    A_R = [A_R; AA(2*i,:)];
end

% plot data
y_max = max(abs(A));
nSample_toplot = nSample_bin;
for i = 1:nAmbiChannels
    index_ir = i;
    subplot(nAmbiChannels,2,1 + 2*(i-1)),plot(A_L(index_ir,1:nSample_toplot)), title(sprintf('Left, Ambi channel: %ld, RMS=%0.5f', i, rms(A_L(index_ir,:)))); xlabel('time'), ylabel('amp');
    axis([1 nSample_toplot -y_max y_max]);
    subplot(nAmbiChannels,2, 2*i),plot(A_R(index_ir,1:nSample_toplot)), title(sprintf('Right, Ambi channel: %ld, RMS=%0.5f', i, rms(A_R(index_ir,:)))); xlabel('time'), ylabel('amp');
    axis([1 nSample_toplot -y_max y_max]);
%     if i < nAmbiChannels; input(''); end;
end;

%% Check: create "ambisonic source" (source + ambisonic encode)

% load audio file
load handel; audioIn = y; clear y;

% Ambisonic encode
src_dir = [-90 0]; % [azim elev]
hoasig = encodeHOA_N3D(ambisonicOrder, audioIn, src_dir); % WYZX

%% Check: decode with loaded HOA2BIN filters

% convole
MAXRE_ON = 1; % Max-rE weighting On/Off, per-order weighting of the decoder, resulting in maximum-norm energy vectors
decoder = 'allrad'; % ALL-ROUND AMBISONIC DECODING

clear left, clear right,
for i = 1: size(hoasig,2)
    left(:,i) = conv(A_L(i, :), hoasig(:, i)');
    right(:,i) = conv(A_R(i, :), hoasig(:, i)');
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








