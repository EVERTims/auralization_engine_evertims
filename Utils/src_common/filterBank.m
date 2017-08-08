function [filterCoefs, Fc_vect] = filterBank(Nbands, Fs)

% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %
% 
%     This script is part of the EVERTims Sound Engine framework
% 
%     Filter Bank Creation
% 
%     Author: David Poirier-Quinot
%     IRCAM, 2017
% 
% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %

%% Args
peakGain = 0.0;
FminMax = [31.5, 20000];
Q = sqrt(2)/2;

%% Get Filters central frequencies
Fc_vect = logspace(log10(FminMax(1)), log10(FminMax(2)), Nbands);

%% Create filters

filterCoefs = zeros(Nbands, 6);
type = 'bandpass';
for i = 1:Nbands;
    [a0, a1, a2, b1, b2] = calcBiquad(type, Fc_vect(i), Fs, Q, peakGain);
    filterCoefs(i, 1:6) = [a0, a1, a2, 1, b1, b2];
end

% % plot filters
% clf, colorList = {'red', 'blue'};
% for i = 1:Nbands;
%     freqz(filterCoefs(i, 1:3),filterCoefs(i, 4:6)); hold on, 
%     lines = findall(gcf,'type','line');
%     set(lines(1),'color',colorList{mod(i,length(colorList))+1} );
% end
% % set x axis log scale
% ax = findall(gcf, 'Type', 'axes');
% set(ax, 'XScale', 'log');
% set(ax, 'YLim', [-10 10]);

% %% Simulate filtering
% x = rand(44100, 1)-0.5;
% % filter stimulus
% for i = 1:N;
%     x_filt = filter(AB(i, 1:3),AB(i, 4:6), x);
%     simpleFFT(x_filt, Fs); input('');
% end
