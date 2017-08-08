% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %
% 
%     This script is part of the EVERTims Sound Engine framework
% 
%     sabine based estimation of room response time
% 
%     Author: David Poirier-Quinot
%     IRCAM, 2017
% 
% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %

% % define room: large
% w = 20; % width
% l = 15; % length
% h = 8; % height

% define room: small
w = 3.8; % width
l = 2.82; % length
h = 2; % height

a = 0.07; % absorption

% get room surface and volume
S = 2*( w*l + w*h + l*h);
V = w*l*h;

RT60 = 0.161 * V / (S*a)
