// inlets and outlets
inlets = 1;
outlets = 2;

// incr. steps
var azimStep = 10;
var elevStep = 10;

// init values
var azimInit = 0 - azimStep; // so it's zero at first bang
var elevInit  = -90;
var maxElev = 90;

// current values
var azim = azimInit; 
var elev = elevInit;

// iterate
function bang() 
{

azim += azimStep
if( azim >= 360 ){
	elev += elevStep;
	azim = 0;
}
if( elev > maxElev ){
	outlet(1, bang)
}
else{
	outlet(0,azim, elev);
}

}

// reset
function msg_int()
{
azim = azimInit;
elev = elevInit;
}