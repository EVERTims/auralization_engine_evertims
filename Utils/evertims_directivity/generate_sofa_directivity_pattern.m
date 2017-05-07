% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %
% 
%     This script is part of the EVERTims Sound Engine framework
% 
%     Create Source / Listener directivity pattern in the SOFA format
% 
%     SOFA processing dependencies: 
%     https://github.com/sofacoustics/API_MO.git
% 
%     Author: David Poirier-Quinot
%     IRCAM, 2017
% 
% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %

%% INIT

% set current folder to script's location
cd( fileparts( mfilename('fullpath') ) ); 

%% Load reference structure
% filename_in = 'MIT_KEMAR_large_pinna.sofa';
filename_in = 'Pulse.sofa';
% filename_in = 'dtf_nh14.sofa';
sofa_struct_ref = SOFAload(filename_in);

%% Init SOFA struct
% create structure
sofa_struct = SOFAgetConventions('SimpleFreeFieldHRIR');

% init fields
filename = 'omni.sofa';
sofa_struct.GLOBAL_Comment = 'Directivity Magnitude across frequency bands, not using GeneralTF awaiting libmysofa update';
sofa_struct.GLOBAL_History = 'Created with the generate_sofa_directivity_pattern.m script for EVERTims';
sofa_struct.GLOBAL_Organization = 'EVERTims';
sofa_struct.GLOBAL_Title = 'OmniDirectional';

%% Create measurement grid
gridStep = 15;
if( mod(180, gridStep) > 0 ); error('script designed for 180 = n*gridStep'); end

azim_v = 0:gridStep:360-gridStep; % all but last (360=0)
elev_v = -90+gridStep:gridStep:90-gridStep; % all but first and last, added afterwards

srcPos = zeros( length(elev_v)*length(azim_v), 3 );

for elevId = 1:length(elev_v);
    for azimId = 1:length(azim_v);
        index = azimId + (elevId-1)*length(azim_v);
        srcPos(index,:) = [ azim_v(azimId) , elev_v(elevId) , 1 ];
    end
end

% add first / last
srcPos = [ [0,-90,1]; srcPos];
srcPos = [ srcPos; [0,90,1] ];

% add to struct
sofa_struct.SourcePosition = srcPos;

% plot
[X,Y,Z] = sph2cart( deg2rad(srcPos(:,1)), deg2rad(srcPos(:,2)), srcPos(:,3) );
plot3( X, Y, Z, '.', 'MarkerSize', 15 ); title(sprintf('measurement grid step: %ld deg', gridStep)); grid;

%% Create directivity pattern

% define frequency bands (NOT STORED SINCE USING SimpleFreeFieldHRIR, TODO when switching to GeneralTF)
% freq_v = [ 300; 4000; 8000 ];
% sofa_struct.N = freq_v; % not 100% sure that's where it goes...
N = 10; % num frequency bands

% define directivity values
r = ones( size(sofa_struct.SourcePosition,1), N );
% omni = r(:,1);
% directional = (1 + cosd( srcPos(:,1) )) .* cosd( srcPos(:,2) );
% % more and more diretional as freq increases
% for i = 1:N;
%     g = (i-1) / (N-1); fprintf('%ld %ld \n', i, g);
%     r(:,i) =  g * directional + (1-g) * omni;
% end
i = 0*r;

% r(:,2) = r(:,3);
% r(:,2) = r(:,3);

% plot
freqId = 10;
[X,Y,Z] = sph2cart( deg2rad(srcPos(:,1)), deg2rad(srcPos(:,2)), r(:, freqId));
plot3( X, Y, Z, '.', 'MarkerSize',15 ); title('real'); grid;

% add to struct
mRn_v = zeros(length(r), 2, N);
sofa_struct.Data.IR = mRn_v;
sofa_struct.Data.IR(:,1,:) = r; % using left ear to store real
sofa_struct.Data.IR(:,2,:) = i; % using left ear to store imag

%% Update Dimensions info
sofa_struct.API.M = size(sofa_struct.SourcePosition,1);
sofa_struct.API.N = N;

sofa_struct.API.Dimensions.ListenerPosition = {'IC'};
sofa_struct.API.Dimensions.ReceiverPosition = {'RC'};
sofa_struct.API.Dimensions.SourcePosition = {'MC'};
sofa_struct.API.Dimensions.EmitterPosition = {'EC'};
% sofa_struct.API.Dimensions.Data.Real = {'mRn'};
% sofa_struct.API.Dimensions.Data.Imag = {'MRN'};
sofa_struct.API.Dimensions.Data.IR = {'MRN'};
sofa_struct.API.Dimensions.Data.Delay = {'IR'};

% %% Adapt to C++ sofa API (carpentier) (false?) libsofa requirements`
% sofa_struct.N_LongName = 'hertz';

%% Adapt to libmysofa (piotr)
sofa_struct.ReceiverPosition = [0 -0.09 0; 0 0.09 0]; % to comply with forced coord check in libmysofa

%% save output
SOFAsave(filename,sofa_struct);

%% reload and check data
% % reload
% clearvars -except filename
% sofa_struct = SOFAload(filename);
% % extract
% srcPos = sofa_struct.SourcePosition;
% r = squeeze(sofa_struct.Data.Real);
% % plot
% freqId = 1;
% [X,Y,Z] = sph2cart( deg2rad(srcPos(:,1)), deg2rad(srcPos(:,2)-90), r(:, freqId));
% plot3( X, Y, Z, '.', 'MarkerSize',15 ); title('real reloaded'); grid;



%% Check coordinates from EVERTims auralization engine printInfo() method
% aedir = [ azim1, elev1, dist1, real1, imag1; ... azimN, elevN, distN, realN, imagN]
[X,Y,Z] = sph2cart( deg2rad(aedir(:,1)), deg2rad(aedir(:,2)), aedir(:,4));
% [X,Y,Z] = sph2cart( deg2rad(aedir(:,1)), deg2rad(aedir(:,2)), ones(size(aedir,1), 1));
plot3( X, Y, Z, '.', 'MarkerSize',15 ); title('real reloaded'); grid;

% for i=1:length(sofa_struct.SourcePosition); fprintf('%ld %ld %ld %ld \n', i-1, sofa_struct.SourcePosition(i,1), sofa_struct.SourcePosition(i,2), sofa_struct.SourcePosition(i,3)); end

%% generate uniform on sphere
% % parameter, greater than 1
% %--------------------------------------------------
% N = 10;
% 
% % compute points
% %--------------------------------------------------
% if N<2, error('Can''t perform tesselation!'); end
% 
% th = 0; ph = 0;
% theta = linspace(0,pi/2,N);
% for t = 2:length(theta)
%   nSlice = 4*(t-1);
%   thSlice = theta(ones(1,nSlice),t).';
%   phSlice = linspace(0,2*pi,nSlice+1);
%   th = [th thSlice];
%   ph = [ph phSlice(1:end-1)];
% end
% 
% theta = linspace(pi/2,pi,N);
% for t = 2:length(theta)-1
%   nSlice = 4*(N-t);
%   thSlice = theta(ones(1,nSlice),t).';
%   phSlice = linspace(0,2*pi,nSlice+1);
%   th = [th thSlice];
%   ph = [ph phSlice(1:end-1)];
% end
% th = [th pi].';
% ph = [ph 0].';
% 
% x = sin(th).*cos(ph);
% y = sin(th).*sin(ph);
% z = cos(th);
% 
% % visualize result
% %--------------------------------------------------
% clf
% trisurf(convhulln([x,y,z]),x,y,z,1);
% hold on
% plot3(x,y,z,'r.');
% axis equal off
% rotate3d