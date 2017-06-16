% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %
% 
%     This script is part of the EVERTims Sound Engine framework
% 
%     Create HOA to Binaural decoding filters based on the 
%     virtual speaker approach.
% 
%     Input:
%     Either LISTEN or SOFA HRTF
%     LISTEN processing dependencies: None
%     SOFA processing dependencies: https://github.com/sofacoustics/API_MO.git
% 
%     Ambisonic Encoding:
%     Either IRCAM or DEFAULT
%     IRCAM depedencies: Private
%     DEFAULT dependencies: https://github.com/polarch/Higher-Order-Ambisonics
%     (+ its own dependencies)
% 
%     How to use:
%     drop SOFA or LISTEN HRIR files in ./input/hrir, run the script, 
%     get resulting hoa to binaural filters in output/hoa2binIRs.
% 
%     Author: David Poirier-Quinot
%     IRCAM, 2016 
% 
% % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %

%% INIT: Set options

clear all;
FORCE_REPROCESSING = true; % will re-create IRs for HRIRs already processed
DEBUG_PLOT = true;


%% INIT: Set paths

% set current folder to script's location
cd( fileparts( mfilename('fullpath') ) ); 

% add subfolders of current dir in search path
root_path = pwd;
addpath( genpath( root_path ) );

% define input/ouptut file names
output_path = fullfile(root_path,'output');
input_path = fullfile(root_path,'input');

%% INIT: Set conversion parameters

% define Ambisonic parameters
ambisonicDecoderOrder = 2;
ambisonicDecoderMethod = 'allrad';
ambisonicDecoder_rEweight = 0;
ambisonicDecoderRawSpeakerArray = true; % uses all HRIR measurement points as virtual speakers if true

%% INIT: List HRIR files to be processed

hrir_filelist = getListofFiles(input_path,'.mat');
hrir_filelist = [hrir_filelist; getListofFiles(input_path,'.sofa')];
if ( size(hrir_filelist, 1) == 0 ) 
    error('found neither .mat nor .sofa file in %s', input_path);
end

%% MAIN: Load HRIRs, create and save HOA to Binaural IRs
for i = 1:length(hrir_filelist);
    
    % check if hrir has already been processed
    [~,filename,~] = fileparts( hrir_filelist(i).name );
    filename_out = fullfile( output_path, [ filename '.bin' ]);
    
    % if indeed it has
    if exist( filename_out, 'file' ) == 2;
        if ~FORCE_REPROCESSING; 
            % skip processing
            fprintf('skipping file (already processed): \n-> %s \n', filename_in);
            continue
        else
            % delete existent and proceed
            delete(filename_out);
            fprintf('deleting old file: \n-> %s \n', filename_out);
        end
    end
    
    % load hrir from file
    filename_in = fullfile( input_path, hrir_filelist(i).name );
    fprintf('loading file: %s \n', filename_in);
    [~, ~, fileExtension] = fileparts( filename_in );
    
    % load from LISTEN file
    if ( strcmp(fileExtension, '.mat') )
        % load hrir (LISTEN)
        load(filename_in);
        % handle naming difference between raw and equalized hrirs
        if ~exist('l_hrir_S', 'var'); 
            l_hrir_S = l_eq_hrir_S; clear l_eq_hrir_S
            r_hrir_S = r_eq_hrir_S; clear r_eq_hrir_S
        end
    % load from SOFA file
    else
        % load hrir (SOFA)
        sofa_struct = SOFAload(filename_in);
        [l_hrir_S, r_hrir_S] = convertSOFA2LISTEN(sofa_struct);
        clear sofa_struct
    end
    
    % check HRIR compliance with EVERTims Sound Engine implementation
    if (l_hrir_S.sampling_hz ~= 44100);
        warning('EvertSE not yet ready for non 44100Hz IRs, use at your own risk');
    end
    
    % plot hrir measurement grid
    if DEBUG_PLOT;
        subplot(2,2,[1 3]),
        plot3(cosd(l_hrir_S.elev_v).*cosd(l_hrir_S.azim_v), ...
              cosd(l_hrir_S.elev_v).*sind(l_hrir_S.azim_v), ...
              sind(l_hrir_S.elev_v), '.', 'MarkerSize', 10);
        title(sprintf('%s measurement grid', hrir_filelist(i).name));
        grid on,
    end
    
    % Get virtual speaker array
    if ambisonicDecoderRawSpeakerArray;
        % Every HRIR in the set is considered as a virtual speaker. Non-optimal
        % method, yet some prefer how the resulting hoa irs sound. use at your
        % own risk.
        l_hrirs_closest = l_hrir_S.content_m';
        r_hrirs_closest = r_hrir_S.content_m';
        dirsAziElev  = [l_hrir_S.azim_v, l_hrir_S.elev_v];
        ls_dirs_rad = deg2rad(dirsAziElev);
    else

        % Define virtual speaker array based on HRIR set measurement grid
        [~, ls_dirs_rad_orig] = getTdesign(2*ambisonicDecoderOrder);

        % Find closest HRIRs in the set and return actual directions
        [l_hrirs_closest, ls_dirs_rad] = getClosestHRIRs(l_hrir_S.content_m.', dirsAziElev, ls_dirs_rad_orig);
        [r_hrirs_closest, ~] = getClosestHRIRs(r_hrir_S.content_m.', dirsAziElev, ls_dirs_rad_orig);
        ambisonicDecoder_rEweight = 1;
    end
    
    % Get HOA decoding matrix for defined speaker array
    M_dec = ambiDecoder ( rad2deg(ls_dirs_rad), ambisonicDecoderMethod, ambisonicDecoder_rEweight, ambisonicDecoderOrder );
        
    % Check if decoding matrix size is correct
    if ( size(M_dec) ~= size(ls_dirs_rad,1) );
        warning('incomplete decoding matrix, switching back to non raw speaker array');
        % Define virtual speaker array based on HRIR set measurement grid
        [~, ls_dirs_rad_orig] = getTdesign(2*ambisonicDecoderOrder);

        % Find closest HRIRs in the set and return actual directions
        [l_hrirs_closest, ls_dirs_rad] = getClosestHRIRs(l_hrir_S.content_m.', dirsAziElev, ls_dirs_rad_orig);
        [r_hrirs_closest, ~] = getClosestHRIRs(r_hrir_S.content_m.', dirsAziElev, ls_dirs_rad_orig);
        ambisonicDecoder_rEweight = 1;    
        
        % Get HOA decoding matrix
        M_dec = ambiDecoder ( rad2deg(ls_dirs_rad), ambisonicDecoderMethod, ambisonicDecoder_rEweight, ambisonicDecoderOrder );        
    end
        
%     % Adapt Ambisonic convention to the one used in the C++ library 
%     ambi_ch_order_matlab = {'W','Y','Z','X','V','T','R','S','U','Q','O','M','K','L','N','P'};
%     ambi_ch_order_cpp = {'W','X','Z','Y', 'V', 'T', 'R', 'S', 'U'};
% %     { 0, 3, 1, 2, 4, 5, 6, 7, 8 }
%     clear reIndexDecodingMatrix;
%     for j = 1:length(ambi_ch_order_cpp)
%         reIndexDecodingMatrix(j) = find(ismember(ambi_ch_order_cpp, ambi_ch_order_matlab{j}));
%     end
%     reIndexDecodingMatrix = [1 2 3 4 5 6 7 8 9]; % SOMEHOW THIS WORKS, I m sure of ambi_ch_order_cpp, ambi_ch_order_matlab must be wrong..?
%     % reIndexDecodingMatrix = [ 0, 2, 3, 1, 8, 7, 6, 5, 4 ] + 1; % correct but x inversed
%     M_dec = M_dec(:,reIndexDecodingMatrix);
    
    % adapt coordinates conv. between Ambisonic lib and HRIR database:
    % elev: ok
    % azim: 0? center ok, Ambi: 90 is left, HRIR: 90 is right (SPAT like)
    M_dec = rotateHOA_N3D(M_dec, 180, 0, 0);
     
    % Combine Ambisonic decoding with HRIRs to get HOA2Bin IRs
    nHRIR = size(M_dec,2);
    lHRIR = size(l_hrirs_closest,1);

    l_hoa2bin = zeros(lHRIR, nHRIR);
    r_hoa2bin = zeros(lHRIR, nHRIR);

    l_hoa2bin(:,:) = l_hrirs_closest * M_dec;
    r_hoa2bin(:,:) = r_hrirs_closest * M_dec;
    
    Fs = l_hrir_S.sampling_hz;
    time_v = 0: 1/Fs : (size(l_hoa2bin,1)-1)/Fs;    
    
    % truncate IR (TO BE REMOVED)
    warning('Ugly (i.e. manual) HRIR truncating is happening there');
    
    firstOnset_index = 1e4; % arbitrary big
    ONSET_THRESHOLD = 1e-1;
    for j = 1:size(l_hoa2bin,2);
        normedDifferential = abs(diff(l_hoa2bin(:,j))) / max(abs(diff(l_hoa2bin(:,j))));
        firstOnset_index = min(firstOnset_index, min( find( normedDifferential > ONSET_THRESHOLD ) ));
        
        normedDifferential = abs(diff(r_hoa2bin(:,j))) / max(abs(diff(r_hoa2bin(:,j))));
        firstOnset_index = min(firstOnset_index, min( find( normedDifferential > ONSET_THRESHOLD ) ));        
    end
    
    IR_DURATION = 0.5e-2; % in sec
    truncateBegin_index = max(1, firstOnset_index-10); % truncate a few samples before
    truncateBegin_sec = (truncateBegin_index - 1) / Fs;
    truncateEnd_sec = min(truncateBegin_sec + IR_DURATION, size(l_hoa2bin,1)/Fs); % make sure it doesn't go beyond IR duration
    truncateEnd_index = ceil(truncateEnd_sec * Fs);
  
    l_hoa2bin = l_hoa2bin(truncateBegin_index:truncateEnd_index, :);
    r_hoa2bin = r_hoa2bin(truncateBegin_index:truncateEnd_index, :);
    
    fprintf('L / R IRs duration (in samples): %ld / %ld \n', size(l_hoa2bin,1), size(r_hoa2bin,1));
    
    if DEBUG_PLOT;        
        subplot(2,2,2), 
        plot(time_v(truncateBegin_index:truncateEnd_index), l_hoa2bin), 
        title('Left Hoa2bin IRs'); 
        xlabel('time (sec)'); ylabel('Amplitude (S.I.)');
%         line([truncateBegin_sec truncateBegin_sec], ...
%              [min(min(l_hoa2bin)) max(max(l_hoa2bin))], ...
%              'Color', 'k', 'LineWidth', 1, 'LineStyle', '--');
%         line([truncateEnd_sec truncateEnd_sec], ...
%              [min(min(l_hoa2bin)) max(max(l_hoa2bin))], ...
%              'Color', 'k', 'LineWidth', 1, 'LineStyle', '--');         
        
        subplot(2,2,4), 
        plot(time_v(truncateBegin_index:truncateEnd_index), r_hoa2bin), 
        title('Right Hoa2bin IRs');
        xlabel('time (sec)'); ylabel('Amplitude (S.I.)');
%         line([truncateBegin_sec truncateBegin_sec], ...
%              [min(min(r_hoa2bin)) max(max(r_hoa2bin))], ...
%              'Color', 'k', 'LineWidth', 1, 'LineStyle', '--');
%         line([truncateEnd_sec truncateEnd_sec], ...
%              [min(min(r_hoa2bin)) max(max(r_hoa2bin))], ...
%              'Color', 'k', 'LineWidth', 1, 'LineStyle', '--');         
        
        input('press Enter key to proceed to next HRIR')
    end
    
    % write IR as binary files
    [sParent,sFile,~] = fileparts(filename_out);
    filename_hrir_radical = fullfile(sParent,['hoa2bin_order' int2str(ambisonicDecoderOrder) '_' sFile]);
    filename_hrir_bin = [ filename_hrir_radical '.bin'];
    file_hrir = fopen(filename_hrir_bin,'w');

    for ind_channel = 1:size(l_hoa2bin,2)
        fwrite(file_hrir,l_hoa2bin(:,ind_channel),'float32');
        fwrite(file_hrir,r_hoa2bin(:,ind_channel),'float32');
    end

    fclose(file_hrir);
    fprintf('created: %s \n', filename_hrir_bin);
    
    % write IR as mat files
    filename_hrir_mat = [ filename_hrir_radical '.mat'];
    ambi2binIr.l_m = l_hoa2bin;
    ambi2binIr.r_m = r_hoa2bin;
    ambi2binIr.sampling_hz = l_hrir_S.sampling_hz;
    save(filename_hrir_mat, 'ambi2binIr');
    fprintf('created: %s \n', filename_hrir_bin);
end
