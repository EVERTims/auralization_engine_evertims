# this script is a simple wget to download default hrirs from the SOFA / LISTEN databases

# download sofa
wget http://sofacoustics.org/data/database/clubfritz/ClubFritz1.sofa

# download LISTEN
LISTEN_ID=IRC_1008
wget ftp://ftp.ircam.fr/pub/IRCAM/equipes/salles/listen/archive/SUBJECTS/${LISTEN_ID}.zip
# extract LISTEN .MAT file
unzip ${LISTEN_ID}.zip
mv RAW/MAT/HRIR/${LISTEN_ID}_R_HRIR.mat .
# clean
rm -Rf COMPENSATED && rm -Rf RAW && rm ${LISTEN_ID}.zip