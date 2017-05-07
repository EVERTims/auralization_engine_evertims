% Test reverb tail design for EVERTims Audio Engine
%
% FDN based, v1 is colored / not dense enougth for large rooms, v2 is too
% slow for real-time. Implementation based on Fig.13 of Jot's 1991 paper:
% "DIGITAL DELAY NETWORKS FOR DESIGNING ARTIFICIAL REVERBERATORS".
% 
% [1]: https://www.dsprelated.com/freebooks/pasp/FDN_Reverberation.html
% David Poirier-Quinot, IRCAM 2016

%% Init
% Define FDN parameters
NumFeedbackLine = 16;
NumFreqBands = 3;

% Define Response Time -60dB (time after which impulse reduced to -60dB of its initial power)
% RT60vect = 4*[ 0.39, 0.39, 0.39, 0.35, 0.28, 0.27, 0.26, 0.25, 0.25, 0.25 ]; % Response Time -60dB in sec
RT60vect = 0.99*[ 1 1 1 ];
RT60 = mean(RT60vect);
FsVect = [];
for i = 1:length(RT60vect); FsVect = [FsVect 31.5 * 2^(i-1)]; end
c = 343; % sound speed

% load audio in

% [in,Fs] = audioread('VMH1 Claps & Snares 023.wav');

% [in,Fs] = audioread('VMH1 Claps & Snares 012.wav'); 
% in = in(:,1); % stereo to mono
% in = 0.2 * in / max(abs(in)); % normalization

in = [1; zeros(9, 1)]; Fs = 44100; % impulse response

% fake source images
numSources = 100; roomLength = 10; % in m
Fs = 44100; c = 343; % m.s-1
minDelaySamp = floor(Fs * roomLength / c);
maxDelaySamp = ceil(Fs * 2*roomLength / c);
vectOnes = round( 1 + rand(numSources,1) * (maxDelaySamp - minDelaySamp));
in = zeros(maxDelaySamp, 1);  in(vectOnes) = 1;
plot(in);

%% Define feedback Matrix
% generate unitary matrix of gains (see e.g.
% https://ccrma.stanford.edu/~jos/cfdn/Feedback_Delay_Networks.html)

% X = (randn(NumFeedbackLine) + i*randn(NumFeedbackLine))/sqrt(2);
% [Q,R] = qr(X);
% R = diag(diag(R)./abs(diag(R)));
% A = Q*R;

% g = 0.8;
% A = (g/sqrt(2)) * [0 1 1 0; -1 0 0 -1; 1 0 0 -1; 0 1 -1 0];
% A = AA; % DEBUG

% % hadamard feedback matrix
% A = (1/(2^(sqrt(NumFeedbackLine)/2))) * hadamard(NumFeedbackLine);

% householder matrix as proposed in Jot's thesis, see also 
% https://ccrma.stanford.edu/~jos/pasp/Householder_Feedback_Matrix.html
A = 0.5*(diag([2 2 2 2]) - ones(4,4));
A = 0.0*[A -A -A -A; -A A -A -A; -A -A A -A; -A -A -A A];

% % export matrix
% for i = 1:size(A,1);
%     for j = 1:size(A,2)
%         fprintf('fdnFeedbackMatrix[%ld][%ld] = %.3f;\n', i-1,j-1,A(i,j));
%     end
% end

%% Define feedback delays
% approach based on [1], based on power of prime numbers to avoid
% coloration in the FDN that would result from phased delays (when at least
% two have a common factor)
delaysSamp = primes(1000);
startOffset = 9; step = 1;
delaysSampPrimes = delaysSamp(startOffset:step:step*NumFeedbackLine+startOffset-1);
delaysSamp = delaysSampPrimes.^( 2 ); % prime power
delays = delaysSamp / Fs;

% from https://github.com/royvegard/zita-rev1/blob/master/source/reverb.cc
% delays = [ 20346e-6 24421e-6 31604e-6 27333e-6 22904e-6 29291e-6 13458e-6 19123e-6 ];

% export delays
for i = 1:length(delays);
    fprintf('fdnDelays[%ld] = %.6f;\n', i-1,delays(i));
end

%% Define feedback delays
% % approach based on Pd patch
% D = [0.5 0.54003 0.583265 0.629961 0.680395 0.734867 0.793701 0.857244 0.925875 1]; % in sec
% roomDim = [3, 5, 8]; % width length height (order doesn't matter here)
% minTravelTime = min(roomDim)*0.5/c; 
% maxTravelTime = norm(roomDim)*1.5/c;
% delays = ((D - 0.5)/0.5)*(maxTravelTime - minTravelTime) + minTravelTime;
% delaysSamp = delays * Fs;

%% Check feedback delays
% ensure mode density above Schroeder's threshold
delaysSamp = ceil(Fs * delays);
if( sum(delaysSamp) < 0.15 * Fs * RT60);
    warning('mode density below threshold: %.f \n(should be above %.f)', sum(delaysSamp), 0.15 * Fs * RT60); 
end
fprintf('mode density is %.1f times the threshold \n', sum(delaysSamp) / (0.15 * Fs * RT60) );

% check mean free path assumption
vOverS = (c/(4*Fs))*mean(delaysSamp);
r = 6*vOverS; % edge of a cube room that would fit these dimensions
fprintf('Volume / Surface (estimated) = %.1f -> cube room edge: %.1fm \n', vOverS, r );

% check minimum delay is still above foreseen block size (can't, otherwise
% zipper noise)
blockSize = 512;
if( delaysSamp(1) < blockSize)
   warning('minimum delay below block size: %.1f samples', delaysSamp(1)); 
end
%% Achieving desired reverberation time (define FDN gains)

% based on [1]: want propagation through n60 samples to result in a 60dB attenuation)
k = zeros(NumFreqBands, NumFeedbackLine);
% simulate absorption corresponding to an M-sample delay
% see https://www.dsprelated.com/freebooks/pasp/Lossy_Acoustic_Propagation.html#sec:lossyprop
for i = 1:NumFreqBands % for each frequency band
    % k(i,:) = (1 - 6.9078./(Fs * RT60vect(i) ) ).^delaysSamp;
    k(i,:) =  10.^(-3* delaysSamp / (RT60vect(i)*Fs) );
end
kmean = (1 - 6.9078./(Fs * RT60 ) ).^delaysSamp; % for non-freq specific tests

% % check that indeed we reach 0.001 after Nth times through these gains
% gainCheck60 = k;
% for i = 1:NumFreqBands;
%     gainCheck60(i,:) = k(i,:).^( RT60vect(i) ./ ( delaysSamp / Fs ) );
% end
% subplot(121),plot(20*log10(gainCheck60(:)), '*'); hold on,
% line([1 length(gainCheck60(:))], [-60 -60]); hold off
% title('resulting attenuation after Nth passage through FDN for each band');
% subplot(122),plot(k), title('gains S.I.');
% leg = cellstr(num2str(delaysSamp', 'del = %.1f')); legend(leg);

% % export gains
% for i = 1:NumFeedbackLine;
%     fprintf('fdnGains[%ld] = %.3f;\n', i-1,kmean(i));
% end
% 
% % export gains (freq specific)
% for i = 1:NumFreqBands;
%     for j = 1:NumFeedbackLine;
%         fprintf('fdnGains[%ld][%ld] = %.5f;\n', i-1,j-1,k(i,j));
%     end
% end
%% design freq. specific filters
% % see [1], part: DELAY-LINE DAMPING FILTER DESIGN
% w = FsVect * pi / Fs;
% lineId = 7;
% h = 10.^( -3*delaysSamp(lineId) ./ (RT60vect * Fs) );
% [B,A] = invfreqz(h,w,1,0);
% % [B,A] = stmcb(h, 1, 0);
% freqz(B,A); hold on, plot(w, h, '+r'); hold off

%% Prepare output
out = zeros(length(in) * ceil(1 + RT60), 1);
maxDelay = max( ceil(max(RT60vect)*Fs), max(delaysSamp) );
delayLine = zeros(length(in) + maxDelay, NumFeedbackLine);

%% Compute output
for i = 1:length(delayLine)
    for m = 1:NumFeedbackLine;
        
        % get current delay time in samples
        delaySamp = round(delays(m) * Fs);
        
        % add simple input to delay line
        if( i < length(in) );
            delayLine(i + delaySamp,m) = delayLine(i + delaySamp,m) + k(m) * in(i);
%             if(m==1); 
%                 subplot(211), stem(delayLine(1:i + delaySamp + 20, m));
%                 fprintf('add (init) %.2f at %ld \n', in(i), i + delaySamp);
%                 input('');
%             end
            
        end
        
        if( i > delaySamp);
            % get sample from delay line
            x = A(m,m) * k(m) * delayLine(i - delaySamp,m);
            
            % add delayed feedback to delay line
            delayLine(i,m) = delayLine(i,m) + x;
%             if(m==1); 
%                 subplot(212), stem(delayLine(1:i + delaySamp + 20, m)); 
%                 fprintf('add (own) %.2f at %ld \n', x, i);
%             end

            % add delayed from other delay lines to current FDN
            for mm = 1:length(delays)
                delaySampNeighbor = round(delays(mm) * Fs);
                if( mm ~= m && i > delaySampNeighbor);
                    x = A(m,mm) * kmean(mm) * delayLine(i - delaySampNeighbor,mm);
                    delayLine(i,m) = delayLine(i,m) + x;
                    % if(m==1); fprintf('add (others) %.2f at %ld \n', x, i); end
                end
            end
%             if(m==1); 
%                 subplot(212), hold on, stem(delayLine(1:i + delaySamp + 20, m)); hold off, 
%                 input('');
%             end
        end
        
    end
end

% reconstruct output
out = sum(delayLine,2); % + [in; zeros(size(delayLine,1)-length(in), 1)];

% add direct to now in output
outStraight = [in; zeros(length(out)-(length(in)), 1) ];

%% Plot output
time = [0:length(out)-1]/Fs;
stem(time, out, 'Marker', 'none'); hold on
stem(time, outStraight, 'LineStyle', '--', 'Marker', 'none'), hold off
legend({'in'}, {'out'}); ylabel('ampl. S.I.'); xlabel('time (sec)');

% add "asked" RT60 value to plot
line([RT60 RT60], [min(out) max(out)]);
% add estimated RT60 value to plot 
% (find last time output goes above RT60 threshold)
index60 = 0;
valMax = max(abs(out));
for i = 1:length(out);
    val = abs( out(length(out) - i - 1) );
    if val > 0.001*valMax; 
        index60 = length(out)-i;
        break
    end
end
hold on, plot(index60/Fs, out(index60), '*k'); hold off



%% Render output
out = out + outStraight;
if(max(abs(out)) < 3);
    a = audioplayer(in,Fs);
    playblocking(a);
    a = audioplayer(out,Fs);
    play(a);
end
