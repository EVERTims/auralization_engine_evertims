function [l_hrir_S, r_hrir_S] = convertSOFA2LISTEN(Obj)
% Converts the data into the listen format for itd estimation 

% load data
l_hrir_S.type_s = 'FIR';
l_hrir_S.elev_v = Obj.SourcePosition(:, 2);
l_hrir_S.azim_v = Obj.SourcePosition(:, 1);
l_hrir_S.sampling_hz = Obj.Data.SamplingRate;
l_hrir_S.content_m = squeeze(Obj.Data.IR(:, 1, :));

r_hrir_S.type_s = 'FIR';
r_hrir_S.elev_v = Obj.SourcePosition(:, 2);
r_hrir_S.azim_v = Obj.SourcePosition(:, 1);
r_hrir_S.sampling_hz = Obj.Data.SamplingRate;
r_hrir_S.content_m = squeeze(Obj.Data.IR(:, 2, :));
end