./configure
cd kern/conf
./config ASST2
cd ../compile/ASST2
bmake depend
bmake
bmake install
cd ../../../

cd ../root
if [ "$1" =  "-d" ]
then
  sys161 -w kernel q
else
  sys161 kernel q
fi
cd ..
