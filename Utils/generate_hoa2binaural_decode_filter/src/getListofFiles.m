function fileList = getListofFiles(dirName, fileType)
tempFileList = dir(dirName);

filenames = {tempFileList.name};

k = strfind(filenames, fileType);
ndx = [];
for i = 1:length(k)
    if ~isempty(cell2mat(k(i)))
        ndx = [ndx, i];
    end
end
fileList = tempFileList(ndx);