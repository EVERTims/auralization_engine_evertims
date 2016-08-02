%     This script is part of the EVERTims Sound Engine framework
% 
%     Create HOA to Binaural decoding filters based on the 
%     virtual speaker approach
% 
% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %

%% INIT
% set flags
WRITE_NEW_FILES = true; % ouptuts files if true
FORCE_REPROCESSING = true; % compute interpolation & cie even if .bin files have already been created for a given hrir

% set current folder to script's location
cd(fileparts(mfilename('fullpath'))); 

% add subfolders of current dir in search path
root_path = pwd;
addpath(genpath(root_path));

% define ouptut file names
output_path = fullfile(root_path,'output','hoa2binIRs');
input_path = fullfile(root_path,'input','hrirs');

% define Ambisonic parameters
ambisonicOrder = 2;

%% LOOP THROUGH DATA, SHAPE, COMPUTE ITD, ETC.

% get hrir list
hrir_dirname = fullfile(input_path);
% hrir_filelist = getListofFiles(hrir_dirname,'.sofa');
hrir_filelist = getListofFiles(hrir_dirname,'.mat');

for i = 1:length(hrir_filelist);
    
    % check if hrir has already been processed
    [~,filename,~] = fileparts(hrir_filelist(i).name);
    filename_out = fullfile(output_path,[filename '.bin']);
    if exist(filename_out, 'file') == 2;
        if ~FORCE_REPROCESSING; % skip processing
            fprintf('skipping file (already processed): \n-> %s \n', filename_in);
            continue
        end
        if WRITE_NEW_FILES; % delete existent
            delete(filename_out);
            fprintf('deleting old file: \n-> %s \n', filename_out);
        end
    end
    
    % load hrir (LISTEN for now, check LSE code for SOFA import)
    filename_in = fullfile(hrir_dirname,hrir_filelist(i).name);
    fprintf('loading file: %s \n', filename_in);
    load(filename_in);
    
    % for SOFA, use:
    % sofa_struct = SOFAload(filename_in);
    % [l_hrir_S, r_hrir_S] = convertSOFA2LISTEN(sofa_struct);        
    
    dirsAziElev  = [l_hrir_S.azim_v, l_hrir_S.elev_v];
    plot3(cosd(l_hrir_S.elev_v).*cosd(l_hrir_S.azim_v), ...
          cosd(l_hrir_S.elev_v).*sind(l_hrir_S.azim_v), ...
          sind(l_hrir_S.elev_v), '.', 'MarkerSize', 10);
% ------------------------------------------------------
% ------------------------------------------------------    
    % Get virtual speaker set
    % Every HRIR in the set is considered as a virtual speaker. Non-optimal
    % method, yet some prefer how the resulting hoa irs sound. use at your
    % own risk.
    l_hrirs_closest = l_hrir_S.content_m';
    r_hrirs_closest = r_hrir_S.content_m';
    ls_dirs_rad = deg2rad(dirsAziElev);
    rE_WEIGHT = 0;
% ------------------------------------------------------    
%     % Define virtual speaker array based on HRIR set measurement grid
%     [~, ls_dirs_rad_orig] = getTdesign(2*ambisonicOrder);
% 
%     % Find closest HRIRs in the set and return actual directions
%     [l_hrirs_closest, ls_dirs_rad] = getClosestHRIRs(l_hrir_S.content_m.', dirsAziElev, ls_dirs_rad_orig);
%     [r_hrirs_closest, ~] = getClosestHRIRs(r_hrir_S.content_m.', dirsAziElev, ls_dirs_rad_orig);
%     rE_WEIGHT = 1;
% ------------------------------------------------------
% ------------------------------------------------------
    % Get HOA decoding matrix
    method = 'ALLRAD';
    M_dec = ambiDecoder ( rad2deg(ls_dirs_rad), method, rE_WEIGHT, ambisonicOrder );
    
    % Adapt Ambisonic convention to the one used in the C++ library 
    ambi_ch_order_matlab = {'W','Y','Z','X','V','T','R','S','U','Q','O','M','K','L','N','P'};
    ambi_ch_order_cpp = {'W','X','Z','Y', 'V', 'T', 'R', 'S', 'U'};
%     { 0, 3, 1, 2, 4, 5, 6, 7, 8 }
    clear reIndexDecodingMatrix;
    for j = 1:length(ambi_ch_order_cpp)
        reIndexDecodingMatrix(j) = find(ismember(ambi_ch_order_cpp, ambi_ch_order_matlab{j}));
    end
    reIndexDecodingMatrix = [1 2 3 4 5 6 7 8 9]; % SOMEHOW THIS WORKS, I m sure of ambi_ch_order_cpp, ambi_ch_order_matlab must be wrong..?
    % reIndexDecodingMatrix = [ 0, 2, 3, 1, 8, 7, 6, 5, 4 ] + 1; % correct but x inversed
    
    M_dec = M_dec(:,reIndexDecodingMatrix);
    
    % Combine Ambisonic decoding with HRIRs

    nHRIR = size(M_dec,2);
    lHRIR = size(l_hrirs_closest,1);

    l_hoa2bin = zeros(lHRIR, nHRIR);
    r_hoa2bin = zeros(lHRIR, nHRIR);

    l_hoa2bin(:,:) = l_hrirs_closest * M_dec;
    r_hoa2bin(:,:) = r_hrirs_closest * M_dec;
    
    % truncate HRIR (TO BE REMOVED)
    TRUNCATE_INDEX_BEGIN = 240;
    TRUNCATE_INDEX_END = 400;
    l_hoa2bin = l_hoa2bin(TRUNCATE_INDEX_BEGIN:TRUNCATE_INDEX_END, :);
    r_hoa2bin = r_hoa2bin(TRUNCATE_INDEX_BEGIN:TRUNCATE_INDEX_END, :);
    subplot(211), plot(l_hoa2bin), subplot(212), plot(r_hoa2bin);
    
    % save data
    if WRITE_NEW_FILES;
        [sParent,sFile,~] = fileparts(filename_out);

        filename_hrir = fullfile(sParent,['hoa2bin_order' int2str(ambisonicOrder) '_' sFile '.bin']);

        file_hrir = fopen(filename_hrir,'w');
        
        for ind_channel = 1:size(l_hoa2bin,2)
            fwrite(file_hrir,l_hoa2bin(:,ind_channel),'float32');
            fwrite(file_hrir,r_hoa2bin(:,ind_channel),'float32');
        end

        fclose(file_hrir);
        fprintf('created: %s \n', filename_hrir);
    else
        fprintf('### No new file written ### (set WRITE_NEW_TABLE flag to true if that was your intention).\n');
    end

end
