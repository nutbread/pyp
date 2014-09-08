# Build settings file
import os;

# Sources
sources = [
	r"Main.c",
	r"PypTags.c",
	r"PypProcessing.c",
	r"PypReader.c",
	r"PypDataBuffer.c",
	r"PypDataBufferModifiers.c",
	r"PypModule.c",
	r"Memory.c",
	r"Map.c",
	r"CommandLine.c",
	r"Unicode.c",
	r"Path.c",
	r"File.c",
];
resources = [
	r"Resources.rc",
];

# Customizable paths
script_dir = os.path.dirname(os.path.realpath(__file__));
project_directories = {
	"src": os.path.join(script_dir, "src"),
	"bin": os.path.join(script_dir, "bin"),
	"res": os.path.join(script_dir, "res"),
	"obj": os.path.join(script_dir, "obj"),
};
python_versions = {
	"2": {
		"architectures": {
			"x86": {
				"library": "python27",
				"path": r"Z:\Code\Python2"
			},
			"x64": {
				"library": "python27",
				"path": r"Z:\Code\Python2x64"
			},
		},
	},
	"3": {
		"architectures": {
			"x86": {
				"library": "python34",
				"path": r"Z:\Code\Python3"
			},
			"x64": {
				"library": "python34",
				"path": r"Z:\Code\Python3x64"
			},
		},
	},
};
compilers = {
	"gcc": {
		"compiler_class": "gcc",
		"architectures": {
			"x86": {
				"compiler": r"Z:\Code\MinGW\bin\gcc",
				"linker": r"Z:\Code\MinGW\bin\gcc",
				"windres": r"Z:\Code\MinGW\bin\windres",
			},
			"x64": {
				"compiler": r"Z:\Code\MinGW64\bin\gcc",
				"linker": r"Z:\Code\MinGW64\bin\gcc",
				"windres": r"Z:\Code\MinGW64\bin\windres",
				"compiler_flags": {
					"debug": [ "-DMS_WIN64", ],
					"release": [ "-DMS_WIN64", ],
				},
			},
		},
		"compiler_flags": {
			"debug": [ "-Wall", "-Werror", "-Wno-error=unused-function", "-g", "-O0", ],
			"release": [ "-Wall", "-Werror", "-Wno-error=unused-function", "-O3", "-DNDEBUG", ],
		},
		"linker_flags": {
			"debug": [],
			"release": [],
		},
		"linker_libraries": {
			"debug": [ "Shell32", ],
			"release": [ "Shell32", ],
		},
	},
	"vc9": {
		"compiler_class": "vc",
		"architectures": {
			"x86": {
				"setup": {
					"debug": [ r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat", ],
					"release": [ r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat", ],
				},
				"linker_flags": {
					"debug": [ "/MACHINE:X86", ],
					"release": [ "/MACHINE:X86", ],
				},
				"cvtres_flags": {
					"debug": [ "/MACHINE:X86", ],
					"release": [ "/MACHINE:X86", ],
				},
				"compiler": r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\cl",
				"linker": r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\link",
				"cvtres": r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\cvtres",
				"rc": r"Z:\Programs\Microsoft SDKs\Windows\v6.1\Bin\RC",
			},
			"x64": {
				"setup": {
					"debug": [ r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\vcvars64.bat", ],
					"release": [ r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\vcvars64.bat", ],
				},
				"linker_flags": {
					"debug": [ "/MACHINE:X64", ],
					"release": [ "/MACHINE:X64", ],
				},
				"cvtres_flags": {
					"debug": [ "/MACHINE:X64", ],
					"release": [ "/MACHINE:X64", ],
				},
				"compiler": r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\amd64\cl",
				"linker": r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\amd64\link",
				"cvtres": r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\amd64\cvtres",
				"rc": r"Z:\Programs\Microsoft SDKs\Windows\v6.1\Bin\RC",
			},
		},
		"compiler_flags": {
			"debug": [ "/W3", "/Zi", "/TC", ],
			"release": [ "/O2", "/DNDEBUG", ],
		},
		"linker_flags": {
			"debug": [],
			"release": [],
		},
		"linker_libraries": {
			"debug": [ "Shell32", ],
			"release": [ "Shell32", ],
		},
	},
	"vc10": {
		"compiler_class": "vc",
		"architectures": {
			"x86": {
				"setup": {
					"debug": [ r"Z:\Programs\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.Cmd", "/x86", ],
					"release": [ r"Z:\Programs\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.Cmd", "/x86", ],
				},
				"linker_flags": {
					"debug": [ "/MACHINE:X86", ],
					"release": [ "/MACHINE:X86", ],
				},
				"cvtres_flags": {
					"debug": [ "/MACHINE:X86", ],
					"release": [ "/MACHINE:X86", ],
				},
				"compiler": r"Z:\Programs\Microsoft Visual Studio 10.0\VC\bin\cl",
				"linker": r"Z:\Programs\Microsoft Visual Studio 10.0\VC\bin\link",
				"cvtres": r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\cvtres", # VC10's crashes for some reason
				"rc": r"Z:\Programs\Microsoft SDKs\Windows\v7.1\Bin\RC",
			},
			"x64": {
				"setup": {
					"debug": [ r"Z:\Programs\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.Cmd", "/x64", ],
					"release": [ r"Z:\Programs\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.Cmd", "/x64", ],
				},
				"linker_flags": {
					"debug": [ "/MACHINE:X64", ],
					"release": [ "/MACHINE:X64", ],
				},
				"cvtres_flags": {
					"debug": [ "/MACHINE:X64", ],
					"release": [ "/MACHINE:X64", ],
				},
				"compiler": r"Z:\Programs\Microsoft Visual Studio 10.0\VC\bin\amd64\cl",
				"linker": r"Z:\Programs\Microsoft Visual Studio 10.0\VC\bin\amd64\link",
				"cvtres": r"Z:\Programs\Microsoft Visual Studio 9.0\VC\bin\amd64\cvtres", # VC10's crashes for some reason
				"rc": r"Z:\Programs\Microsoft SDKs\Windows\v7.1\Bin\RC",
			},
		},
		"compiler_flags": {
			"debug": [ "/W3", "/Zi", "/TC", ],
			"release": [ "/O2", "/DNDEBUG", ],
		},
		"linker_flags": {
			"debug": [],
			"release": [],
		},
		"linker_libraries": {
			"debug": [ "Shell32", ],
			"release": [ "Shell32", ],
		},
	},
};


