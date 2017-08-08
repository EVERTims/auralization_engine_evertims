% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %
% 
%     This script is part of the EVERTims Sound Engine framework
% 
%     Estimate RT60 of EVERTims generated IR
% 
%     Author: David Poirier-Quinot
%     IRCAM, 2017
% 
% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %

% add misc src path
addpath(genpath(fullfile(fileparts(pwd),'src_common')));

% Load IR
[ir, Fs] = audioread('Evertims_IR_Recording_binaural_10.wav');
ir = ir(:,1);

%% filter bank decomposition
Nbands = 10;
[filterCoefs, Fc_vect] = filterBank(Nbands, Fs);
irBands = zeros(size(ir,1), Nbands);
for i = 1:Nbands;
    irBands(:,i) = filter(filterCoefs(i, 1:3),filterCoefs(i, 4:6), ir);
end

% estimate reverberation time (energy)
rt60 = zeros(Nbands,1);
for i = 1:Nbands;
    % rt60(:,i) = t60(irBands(:,i),Fs,1); input(''); close % debug plot
    rt60(i) = t60(irBands(:,i),Fs,0);
end

% estimate reverberation time (threshold)
rt60bis = zeros(Nbands,1);
for i = 1:Nbands;
    valMax = max(abs( irBands(:,i) ));
    for k = 1:size(irBands,1);
        val = abs( irBands(end-k-1,i) );
        if val > 0.001*valMax;
            rt60bis(i) = (size(irBands,1) - k)/Fs;
            break;
        end
    end
end

time = [1:size(irBands,1)] / Fs;
plot(rt60/1000,'o');
m = mean(rt60)/1000; line([1 Nbands], [m m], 'LineStyle', '--'); % average RT60
% hold on, plot(rt60bis,'*'); title('rt60 (in sec) across freq bands'); hold off
% legend({'energy', 'threshold'});
% line([1 Nbands], [time(end) time(end)], 'LineStyle', '--'); % file duration

%% filter bank plot / listen
% offset = max(max(abs(irBands)));
% for i = 1:Nbands;
%     plot(time, irBands(:,i) + (i-1)*offset), hold on
% end, hold off
% p = audioplayer(irBands(:,7), Fs);
% play(p);