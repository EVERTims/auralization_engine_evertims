function [rt,iidc] = t60(impulse,S_Rate,flag)

%[{rt,iidc}] = t60(impulse,S_Rate,{flag})
%
%Returns an estimate of t60, a measure of reverberation time 
%(in ms) and optionally, the integrated impulse decay curve 
%of the impulse response, using S_Rate as the sample rate
%
%Optional flag is 1 to plot. Default is no plot, unless no output
%arguments are specified
%
%Pass only the segment of the recording after the offset 
%of the impulse
%
%Reference:
%	Schroeder, M.R. (1965). New method of measuring reverberation
%time. Journal of the Acoustical Society of America, 37, 409-412.
%
%Version history:
%
%04-2001 First version
%05-2002 Fixed some bugs. Now accepts a column or row vector.
%			Also, fixed error in which the input vector was being
%			squared twice. Finally, fixed a bug that generated 
%			an error if the rt was longer than 1.5 s. 
%			Thanks to Olivier Deille for his helpful suggestions. 
%
%Christopher Brown, cbrown@phi.luc.edu

if nargin==2
	if nargout==0
		flag = 1;
	else
		flag = 0;
	end
elseif nargin==3
else
	error('Wrong number of input arguements. ''help t60'' for details');
end

if size(impulse,2)==1
impulse = impulse.';
end

begin = round(S_Rate * .05);	%assume slope extends to at least 50 ms
										%adjust this number if needed

if length(impulse)/S_Rate < 1.5	%if impulse is not at least 1.5 sec
   T = length(impulse);				%then use the whole waveform
   warning(['input vector less than 1.5 s in duration']);
else
   T = round(S_Rate * 1.5); 		%otherwise, use first 1.5 sec
end

iidc(length(impulse):-1:1) = cumsum(impulse(length(impulse):-1:1).^2);

y = iidc(1:T);
y = 10 * log10(y/max(abs(y)));	%generate y
xtime = linspace(1,(T/(S_Rate/1000)),T);	%generate x

%calculate r2's
sampsize = 1:length(xtime);
Exy2 = (cumsum(xtime.*y) - ((cumsum(xtime).*cumsum(y))./sampsize)).^2;
Ex2 = cumsum(xtime.^2) - (((cumsum(xtime)).^2)./sampsize);
totss = cumsum(y.^2) - (((cumsum(y)).^2)./sampsize);
regss = Exy2(begin:T)./Ex2(begin:T);
r2 = regss ./ totss(begin:T);

r3 = find(r2 == max(r2));		%find all of the points along slope where r2 is max
stop = begin + r3(length(r3)) - 1;  %stop is the largest r2 that's furthest from t0

mx = mean(xtime(1:stop)); my = mean(y(1:stop));
mx2 = mean(xtime(1:stop).^2); my2 = mean(y(1:stop).^2);
sdx = sqrt(mx2 - mx.^2); sdy = sqrt(my2 - my.^2);
mxy = mean(xtime(1:stop).*y(1:stop));
r = (mxy - (mx.*my))/(sdx.*sdy);
slope = (r * sdy)/sdx;
yintercept = -(slope * mx) + my;

rt = round(abs(60/slope(1))-yintercept);

if flag == 1

	clear xtime;
	xtime = linspace(1,rt,rt*(S_Rate/1000));	%regenerate x
	if length(y)<length(xtime)
		difference = length(xtime)-length(y);
		pad(1:difference) = nan;
		y = [y,pad];
	else
		y = y(1:length(xtime));
	end

	figure;
	plot(xtime,y,'b-','LineWidth',1.5);
	title('Integrated Impulse Decay Curve');
	ylim([-70 0]);
	xlim([0 rt*1.1]);
	xlabel('Time (ms)'), ylabel('dB')
	xlimits = xlim; ylimits = ylim;
	line([xlimits(1),xlimits(2)],[-60,-60],'Color',[.4,.4,.4],'LineWidth',.5);
	text(.6 * xlimits(2),-8,['T_6_0 (ms) = ',num2str(rt)],'FontSize',10,'color','k');
	linestart = (xlimits(1) * slope) + yintercept;
	line([xlimits(1),rt],[linestart,-60],'Color','r','LineWidth',.5);
end