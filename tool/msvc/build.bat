::use in Visual Studio Developer Command Prompt
::call tool/msvc/build.bat

cl.exe src/*.c /I src /I include /EHsc /LD /Fosrc\ /Felib\jml /DEF tool/msvc/jml.def /link /out:lib/jml.dll

cl.exe ext/cli/*.c lib/jml.lib /I src /I include /EHsc /Foext\cli\ /Feext\cli\jml /link /out:bin/jml.exe

for %%f in (.\std\*.c) do (
    cl.exe %%f /I src /I include lib/jml.lib /EHsc /LD /Fostd\ /Festd\%%~nf /link /out:std/%%~nf.dll
)
