function ambi2binIr = loadBinAmbi2Bin(filepath, ambisonicOrder, nSampExpected)

% Load data
fileID = fopen(filepath);
A = fread(fileID,'float32');
fclose(fileID);

% check data
nAmbiChannels = (ambisonicOrder+1)^2;
nChannels = 2; % L / R
nSampReal = size(A,1) / ( nAmbiChannels * nChannels );
if (nSampReal ~= nSampExpected);
    error('announced and real number of samples differ \n expected: %ld \n real: %ld \n', nSampExpected, nSampReal );
end

% shape data 
AA = reshape(A,nSampExpected,[]).';
A_L = []; A_R = [];
for i = 1:size(AA,1)/2;
    A_L = [A_L; AA(2*(i-1)+1,:)];
    A_R = [A_R; AA(2*i,:)];
end

ambi2binIr.l_m = A_L;
ambi2binIr.r_m = A_R;