set src_dir=src
set std_dir=std
set bin_dir=bin
set lib_dir=lib

call :clean_dir %src_dir%
call :clean_dir %std_dir%
call :clean_dir %bin_dir%
call :clean_dir %lib_dir%

del *.obj *.exp *.pdb *.dll *.lib *.ilk jml.exe 2> nul

cd etx/cli

del *.obj *.exp *.pdb *.dll *.lib *.ilk jml.exe 2> nul

exit /b

:clean_dir
    cd %~1
    del *.obj *.exp *.pdb *.dll *.lib *.ilk jml.exe 2> nul
    cd ..
    goto :eof
