% adapted from http://www.earlevel.com/main/2011/01/02/biquad-formulas/
%
% David Poirier-Quinot, IRCAM 2016

function [a0, a1, a2, b1, b2] = calcBiquad(type, Fc, Fs, Q, peakGain)
        
V = 10^(abs(peakGain) / 20);
K = tan(pi * Fc / Fs);

switch (type)
    case 'lowpass'
        norm = 1 / (1 + K / Q + K * K);
        a0 = K * K * norm;
        a1 = 2 * a0;
        a2 = a0;
        b1 = 2 * (K * K - 1) * norm;
        b2 = (1 - K / Q + K * K) * norm;

    case 'highpass'
        norm = 1 / (1 + K / Q + K * K);
        a0 = 1 * norm;
        a1 = -2 * a0;
        a2 = a0;
        b1 = 2 * (K * K - 1) * norm;
        b2 = (1 - K / Q + K * K) * norm;

    case 'bandpass'
        norm = 1 / (1 + K / Q + K * K);
        a0 = K / Q * norm;
        a1 = 0;
        a2 = -a0;
        b1 = 2 * (K * K - 1) * norm;
        b2 = (1 - K / Q + K * K) * norm;

    case 'notch'
        norm = 1 / (1 + K / Q + K * K);
        a0 = (1 + K * K) * norm;
        a1 = 2 * (K * K - 1) * norm;
        a2 = a0;
        b1 = a1;
        b2 = (1 - K / Q + K * K) * norm;

    case 'peak'
        if (peakGain >= 0)    % boost
            norm = 1 / (1 + 1/Q * K + K * K);
            a0 = (1 + V/Q * K + K * K) * norm;
            a1 = 2 * (K * K - 1) * norm;
            a2 = (1 - V/Q * K + K * K) * norm;
            b1 = a1;
            b2 = (1 - 1/Q * K + K * K) * norm;
        
        else    % cut
            norm = 1 / (1 + V/Q * K + K * K);
            a0 = (1 + 1/Q * K + K * K) * norm;
            a1 = 2 * (K * K - 1) * norm;
            a2 = (1 - 1/Q * K + K * K) * norm;
            b1 = a1;
            b2 = (1 - V/Q * K + K * K) * norm;
        end
        
    case 'lowShelf'
        if (peakGain >= 0)    % boost
            norm = 1 / (1 + sqrt(2) * K + K * K);
            a0 = (1 + sqrt(2*V) * K + V * K * K) * norm;
            a1 = 2 * (V * K * K - 1) * norm;
            a2 = (1 - sqrt(2*V) * K + V * K * K) * norm;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - sqrt(2) * K + K * K) * norm;
        
        else    % cut
            norm = 1 / (1 + sqrt(2*V) * K + V * K * K);
            a0 = (1 + sqrt(2) * K + K * K) * norm;
            a1 = 2 * (K * K - 1) * norm;
            a2 = (1 - sqrt(2) * K + K * K) * norm;
            b1 = 2 * (V * K * K - 1) * norm;
            b2 = (1 - sqrt(2*V) * K + V * K * K) * norm;
        
        end
        
    case 'highShelf'
        if (peakGain >= 0)    % boost
            norm = 1 / (1 + sqrt(2) * K + K * K);
            a0 = (V + sqrt(2*V) * K + K * K) * norm;
            a1 = 2 * (K * K - V) * norm;
            a2 = (V - sqrt(2*V) * K + K * K) * norm;
            b1 = 2 * (K * K - 1) * norm;
            b2 = (1 - sqrt(2) * K + K * K) * norm;
        
        else    % cut
            norm = 1 / (V + sqrt(2*V) * K + K * K);
            a0 = (1 + sqrt(2) * K + K * K) * norm;
            a1 = 2 * (K * K - 1) * norm;
            a2 = (1 - sqrt(2) * K + K * K) * norm;
            b1 = 2 * (K * K - V) * norm;
            b2 = (V - sqrt(2*V) * K + K * K) * norm;
        
        end
end