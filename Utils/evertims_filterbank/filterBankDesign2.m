% Test filter bank design for EVERTims JUCE Audio plugin: either 10 or 3
% band filter banks, composed of low pass filters recursively applied to
% the input signal to exctract (recursively then) the frequency bands of
% interest before applying freq-specific absorption gains.
%
% David Poirier-Quinot, IRCAM 2016

%% Init
Fs = 44100;
Q = sqrt(2)/2; %sqrt(2)/(2-1);
peakGain = 0.0;
Fmax = 20000;

%% Generate "10 bands filter bank" Fc
N = 9; % all but the last one

% get Fc list (since we go for lowpass rather than bandpass, we need to 
% find the 'in-between' freq for each Fc of classical octave filter-bank
% NOTE FOR SELF: is it really 'linear in-betwen' we want?
Fc = 31.5; % in Hz
Fc_mid = [];
for i = 2:N;
    Fc = [Fc Fc(i-1)*2];
    Fc_mid(i-1) = ( Fc(i-1) + Fc(i) ) / 2;
end
Fc_mid(i) = Fc(i) + (Fmax - Fc(i)) / 2;
% semilogy(Fc, '*'); hold on, semilogy([1:N]+0.5, Fc_mid, '*'); hold off, 
Fc = Fc_mid;

%% Generate "3 bands filter bank" Fc
N = 2; % all but the last one

Fc = [480, 8200]; % in Hz
Fc_mid = [];
for i = 2:N;
    Fc_mid(i-1) = ( Fc(i-1) + Fc(i) ) / 2;
end
Fc_mid(i) = Fc(i) + (Fmax - Fc(i)) / 2;
semilogy(Fc, '*'); hold on, semilogy([1:N]+0.5, Fc_mid, '*'); hold off, 
Fc = Fc_mid;

%% Create filters

% create filters
AB = [];
type = 'lowpass';
for i = 1:N;
    [a0, a1, a2, b1, b2] = calcBiquad(type, Fc(i), Fs, Q, peakGain);
    AB(i, 1:6) = [a0, a1, a2, 1, b1, b2];
end

% plot filters
clf, colorList = {'red', 'blue'};
for i = 1:N;
    freqz(AB(i, 1:3),AB(i, 4:6)); hold on, 
    lines = findall(gcf,'type','line');
    set(lines(1),'color',colorList{mod(i,length(colorList))+1} );
end
% set x axis log scale
ax = findall(gcf, 'Type', 'axes');
set(ax, 'XScale', 'log');
set(ax, 'YLim', [-10 10]);

%% Simulate filtering

% create stimulus
clf
x = rand(44100, 1)-0.5;

% filter stimulus, emulate per-band energy loss
G = 1.0*ones(N+1,1);
% G(3) = 1;
x_rest = x; y = zeros(size(x));
for i = 1:N;
    x_filt = filter(AB(i, 1:3),AB(i, 4:6), x_rest);
    y = y + x_filt * G(i);
%     simpleFFT(x_filt, Fs); input('');
    x_rest = x_rest - x_filt;
end
% last band
x_filt = x_rest;
% simpleFFT(x_filt, Fs); input('');
y = y + x_filt * G(i+1);

% plot IO
subplot(3,2,1), plot(x); title('in')
subplot(3,2,2), simpleFFT(x, Fs); title('in (freq)')
subplot(3,2,3), plot(y); title('out')
subplot(3,2,4), simpleFFT(y, Fs); title('out (freq)')
subplot(3,2,5), plot(y-x); title('diff')
subplot(3,2,6), simpleFFT(y-x, Fs); title('diff (freq)')

% compare output / input
p = audioplayer([x y], Fs);
play(p);