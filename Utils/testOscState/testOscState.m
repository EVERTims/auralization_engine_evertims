% Import EVERTims state, exported from the auralization engine

fileName = 'Evertims_state.txt';
s = struct;

%% Count number of listener / sources
fid = fopen(fileName);
numList = 0;
numSrc = 0;
goOn = true;
while(goOn);
    C = textscan(fid, '%s%s%s%f%f%f%s%f%f%f%f%f%f%f%f%f', 1);
    if( strcmp(C{1}, 'listener:') );
        numList = numList + 1;
    elseif( strcmp(C{1}, 'source:') );
        numSrc = numSrc + 1;
    else goOn = false;
    end
end
fclose(fid);
fprintf('num listener(s) detected: %ld\n', numList);
fprintf('num source(s) detected: %ld\n', numSrc);

%% Load data
fid = fopen(fileName);

% listeners
for i = 1:numList;
    C = textscan(fid, '%s%s%s%f%f%f%s%f%f%f%f%f%f%f%f%f', 1, 'Delimiter', ' ');
    s.listeners(i).name = C{2};
    s.listeners(i).pos = [ C{4} C{5} C{6} ];
    s.listeners(i).rot = [ C{8} C{9} C{10}; C{11} C{12} C{13}; C{14} C{15} C{16} ];    
end

% sources
for i = 1:numSrc;
    C = textscan(fid, '%s%s%s%f%f%f%s%f%f%f%f%f%f%f%f%f', 1, 'Delimiter', ' ');
    s.sources(i).name = C{2};
    s.sources(i).pos = [ C{4} C{5} C{6} ];
    s.sources(i).rot = [ C{8} C{9} C{10}; C{11} C{12} C{13}; C{14} C{15} C{16} ];    
end

% response time
C = textscan(fid, '%s%f%f%f%f%f%f%f%f%f%f', 1, 'Delimiter', ' ');
s.rt60 = [ C{2} C{3} C{4} C{5} C{6} C{7} C{8} C{9} C{10} C{11} ];

% image sources
% imgSrc: 0 order: 0 posFirst: -1.08 -0.61 -0.39 posLast: 0.12 -0.01 -0.76 dist: 1.39173
C = textscan(fid, '%s%d%s%d%s%f%f%f%s%f%f%f%s%f', 'Delimiter', ' ');
numImgSrc = length(C{1});
fprintf('num image source(s) detected: %ld\n', numImgSrc);
for i = 1:numImgSrc;
    s.imgSrc(i).id = C{2}(i);
    s.imgSrc(i).order = C{4}(i);
    s.imgSrc(i).posFirst = [ C{6}(i) C{7}(i) C{8}(i) ];
    s.imgSrc(i).posLast = [ C{10}(i) C{11}(i) C{12}(i) ];
    s.imgSrc(i).pathLength = C{14}(i);
end

fclose(fid);

%% Plot / check data

% check src id
if( [s.imgSrc.id] ~= [0:length(s.imgSrc)-1] ); warning('srcImg index error'); end

% plot order distribution
histogram([s.imgSrc.order]); title('imgSrc order hist.');

% plot path length distribution
histogram([s.imgSrc.pathLength]); title('path length hist.');

for i = 1:length(s.listeners)
    p = [s.listeners(i).pos(1), s.listeners(i).pos(2), s.listeners(i).pos(3)];
    plot3(p(1), p(2), p(3), 'ob');
    text(p(1), p(2), p(3)-0.7, s.listeners(i).name);
    hold on,
end
for i = 1:length(s.sources)
    p = [s.sources(i).pos(1), s.sources(i).pos(2), s.sources(i).pos(3)];
    plot3(p(1), p(2), p(3), 'or');
    text(p(1), p(2), p(3)-0.7, s.sources(i).name);
    hold on,
end
for i = 1:length(s.imgSrc)
    plot3(s.imgSrc(i).posFirst(1), s.imgSrc(i).posFirst(2), s.imgSrc(i).posFirst(3), '.b');
    plot3(s.imgSrc(i).posLast(1), s.imgSrc(i).posLast(2), s.imgSrc(i).posLast(3), '.r');
end
hold off