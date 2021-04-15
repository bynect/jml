::use in Visual Studio Developer Command Prompt
::call tool/msvc/build.bat

cl.exe src/*.c /I include /EHsc /LD /Fosrc\ /Felib\jml /DEF tool/msvc/jml.def /link /out:lib/jml.dll

cl.exe ext/cli/*.c lib/jml.lib /I include /EHsc /Foext\cli\ /Feext\cli\jml /link /out:bin/jml.exe

cl.exe ext/dis/*.c lib/jml.lib /I include /EHsc /Foext\dis\ /Feext\dis\jbc /link /out:bin/jbc.exe

for /R std %%f in (*.c) do (
    cl.exe %%f /I include lib/jml.lib /EHsc /LD /Fostd\ /Fe%%~dpnf /link /out:%%~dpnf.dll
)
