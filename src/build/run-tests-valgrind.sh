set -e

echo "The script you are running has basename `basename "$0"`, dirname `dirname "$0"`"
echo "The present working directory is `pwd`"

CWD=`dirname "$0"`
echo $CWD
cd $CWD
ls
echo "- Starting API tests"
for file in ./*.test-valgrind.exe;
do
      echo "-- Running test $file"
      valgrind "$file"
      echo "-- Test complete"
done
echo "- API tests complete!"
