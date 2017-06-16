% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %
% 
%     This script is part of the EVERTims Sound Engine framework
% 
%     Compare Ambisonic gains, to check methods used in EVERTims 
%     auralization engine, compared here to SPAT's. 3rd order for now.
% 
%     (N3D format)
% 
%     Author: David Poirier-Quinot
%     IRCAM, 2017
% 
% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %

%% LOAD DATA

% define paths
gainTablesDir = 'gain_tables';

% load SPAT (IRCAM, reference)
% format: azim elev [ambiGains] (x16, 3rd order)
filename = 'spat_ambi_gains_hor_ver_n3d.txt';
gSpat = dlmread(fullfile(gainTablesDir,filename));
gSpat = flip(gSpat); % flip index (azim, elev ordering) to match others'

% load Ambix (C++ lib used in Evert Auralization Engine)
% format: azim elev [ambiGains] (x16, 3rd order)
filename = 'ambix_ambi_gains_hor_ver_n3d.txt'; 
gAmbix = dlmread(fullfile(gainTablesDir,filename));

% generate Polarch 
% (used to create Ambi2bin decode filters for EVERTims auralization engine)
order = 3; % Ambisonic order
src_directions = gAmbix(:, 1:2); % azim, elev, in degrees
gPolarch = getRSH(order, src_directions);
gPolarch = [src_directions gPolarch.']; % re-add source direction to match others

%% CHECK DIFF

% reorder spat  gains, as its azim 90 is others' zero
ind0 = find(gSpat(:,1) == 0);
ind90 = find(gSpat(:,1) == 90);
gSpat2 = gSpat;
for i = 1:length(ind0)-1;
    blockVect = ind0(i):ind0(i+1)-1;
    gSpat2(blockVect,:) = circshift(gSpat(blockVect,:), -ind90(i)+1, 1);
end

% print diff
s = sum(sum(abs(gPolarch(:,3:end) - gAmbix(:,3:end))));
fprintf('polarch - ambix: %0.1f \n', s);
s = sum(sum(abs(gPolarch(:,3:end) - gSpat2(:,3:end))));
fprintf('polarch - spat: %0.1f \n', s);
s = sum(sum(abs(gAmbix(:,3:end) - gSpat2(:,3:end))));
fprintf('ambix - spat: %0.1f \n', s);

% plot diff
subplot(131), surf(gSpat2(:,3:end) - gAmbix(:,3:end)); title('spat - ambix');
subplot(132), surf(gSpat2(:,3:end) - gPolarch(:,3:end)); title('spat - polarch');
subplot(133), surf(gPolarch(:,3:end) - gAmbix(:,3:end)); title('polarch - ambix');