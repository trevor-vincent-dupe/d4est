echo "Starting Stamm Regression test"
echo ${PWD}
sed 's|num_of_amr_steps = .*|num_of_amr_steps = 9|g' ../Stamm/options.input > stamm_options.input
# RES=$(../Stamm/stamm_driver stamm_options.input | grep -c "0.0000001165842921684272017 0.0000007074379709713716485 0.0000146083448435542040227 0.0001316349774934072013998")
RES=$(../Stamm/stamm_driver stamm_options.input | grep -c "0.0000000013301248793227330 0.0000000171172621537055147 0.0000013032058553497301960 0.0000007629001363926490040")

rm stamm_options.input
if [ $RES -ne "1" ]; then
    echo "Test Fail"
    exit 1
else
    echo "Test Success"
fi


