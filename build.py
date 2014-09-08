import os, sys, re, subprocess;
from settings import *;



# Python 2/3 support
if (sys.version_info[0] == 3):
	# Version 3
	def py_2or3_str_to_bytes(text, encoding="ascii", errors="strict"):
		return bytes(text, encoding, errors);
	def py_2or3_bytes_to_str(text, encoding="ascii", errors="strict"):
		return text.decode(encoding, errors);
	def py_2or3_byte_ord(char):
		return char;
	def py_2or3_is_string(value):
		return isinstance(value, str);
else:
	# Version 2
	def py_2or3_str_to_bytes(text, encoding="ascii", errors="strict"):
		return text.encode(encoding, errors);
	def py_2or3_bytes_to_str(text, encoding="ascii", errors="strict"):
		return text.decode(encoding, errors);
	def py_2or3_byte_ord(char):
		return ord(char);
	def py_2or3_is_string(value):
		return isinstance(value, basestring);



# Convert a list of arguments into a string of arguments which can be executed on the command line
def argument_list_to_command_line_string(arguments, forced):
	args_new = [];
	re_valid_pattern = re.compile(r"^[a-zA-Z0-9_\-\.\+\\/]+$");
	for arg in arguments:
		# Format the argument
		if (forced or re_valid_pattern.match(arg) is None):
			arg = '"{0:s}"'.format(arg.replace('"', '""'));

		# Add
		args_new.append(arg);

	# Join and return
	return " ".join(args_new);



# Validate that the target info is okay
def validate(target_info):
	# Compiler checking
	if (target_info["compiler"] not in compilers):
		return "Invalid compiler";
	if (
		"architectures" not in compilers[target_info["compiler"]] or
		target_info["architecture"] not in compilers[target_info["compiler"]]["architectures"]
	):
		return "Invalid architecture";

	# Python checking
	if (target_info["python"] not in python_versions):
		return "Invalid python version";
	if (
		"architectures" not in python_versions[target_info["python"]] or
		target_info["architecture"] not in python_versions[target_info["python"]]["architectures"]
	):
		return "Invalid python architecture";

	# Done
	return None;



# Execute a command
def run_command(cmd, **kwargs):
	# Run the command
	shell = ("shell" in kwargs and kwargs["shell"]);
	p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=shell);
	c = p.communicate();

	# Error count
	if (p.returncode is None or p.returncode != 0):
		sys.stdout.write("{0:s}\n".format("=" * 80));
		sys.stdout.write("cmd = {0:s}\n".format(cmd if py_2or3_is_string(cmd) else argument_list_to_command_line_string(cmd, False)));
		sys.stdout.write("{0:s}\n".format("=" * 80));
		sys.stdout.write(py_2or3_bytes_to_str(c[0], "utf-8", "ignore"));
		sys.stdout.write("{0:s}\n".format("=" * 80));
		return 1;
	return 0;



# Compile using GCC
def gcc_compile(output_name, target_info):
	# Settings
	descriptor = [
		"py" + target_info["python"],
		target_info["mode"],
		target_info["compiler"],
		target_info["architecture"],
	];
	if (target_info["type"] == "dll"):
		output_name = os.path.join(project_directories["bin"], "{0:s}-{1:s}{2:s}".format(output_name, "-".join(descriptor), ".dll"));
		descriptor.extend([ "dll", ]);
	else:
		output_name = os.path.join(project_directories["bin"], "{0:s}-{1:s}{2:s}".format(output_name, "-".join(descriptor), ".exe"));

	# Set the comipler
	compiler_global_info = compilers[target_info["compiler"]];
	compiler_info = compilers[target_info["compiler"]]["architectures"][target_info["architecture"]];
	compiler = compiler_info["compiler"];
	linker = compiler_info["linker"];
	windres = compiler_info["windres"];



	# Compile flags
	compiler_flags = [
		"-I{0:s}".format(os.path.join(python_versions[target_info["python"]]["architectures"][target_info["architecture"]]["path"], "include")),
	];
	linker_flags = [
		"-L{0:s}".format(os.path.join(python_versions[target_info["python"]]["architectures"][target_info["architecture"]]["path"], "libs")),
	];
	linker_libraries = [
		"-l{0:s}".format(python_versions[target_info["python"]]["architectures"][target_info["architecture"]]["library"]),
	];
	for info in [ compiler_global_info , compiler_info ]:
		if ("compiler_flags" in info and target_info["mode"] in info["compiler_flags"]):
			compiler_flags.extend(info["compiler_flags"][target_info["mode"]]);
		if ("linker_flags" in info and target_info["mode"] in info["linker_flags"]):
			linker_flags.extend(info["linker_flags"][target_info["mode"]]);
		if ("linker_libraries" in info and target_info["mode"] in info["linker_libraries"]):
			linker_libraries.extend([ "-l{0:s}".format(i) for i in info["linker_libraries"][target_info["mode"]] ]);



	# New directory
	object_dir = os.path.join(project_directories["obj"], "-".join(descriptor));
	try:
		os.makedirs(object_dir);
	except OSError:
		pass;



	# Compile object files
	object_filenames = [];
	compilation_errors = 0;
	for input in sources:
		# Setup command
		object_file = os.path.join(object_dir, "{0:s}.obj".format(os.path.splitext(input)[0]));
		object_filenames.append(object_file);

		# Output
		sys.stdout.write("{0:s}: compiling {1:s}\n".format(os.path.splitext(os.path.split(compiler)[1])[0], input));

		# Compile
		cmd = [
			compiler,
			"-c",
		] + compiler_flags + [
			"-o", object_file,
			os.path.join(project_directories["src"], input),
		];
		compilation_errors += run_command(cmd);


	# Compile resources
	for input in resources:
		# Setup command
		res_file = os.path.join(object_dir, "{0:s}.res".format(os.path.splitext(input)[0]));
		coff_file = os.path.join(object_dir, "{0:s}.coff".format(os.path.splitext(input)[0]));
		object_filenames.append(coff_file);

		# Output
		sys.stdout.write("{0:s}: compiling {1:s}\n".format(os.path.splitext(os.path.split(windres)[1])[0], input));

		# Compile res
		cmd = [
			windres,
			"-J", "rc",
			"-O", "res",
			"-o", res_file,
			os.path.join(project_directories["res"], input),
		];
		errors = run_command(cmd);
		if (errors > 0):
			compilation_errors += errors;
			continue;

		# Compile coff
		cmd = [
			windres,
			"-J", "rc",
			"-O", "coff",
			"-o", coff_file,
			os.path.join(project_directories["res"], input),
		];
		compilation_errors += run_command(cmd);


	# Link
	if (compilation_errors == 0):
		# Output
		sys.stdout.write("{0:s}: linking {1:s}\n".format(os.path.splitext(os.path.split(linker)[1])[0], os.path.splitext(os.path.split(output_name)[1])[0]));

		# Link
		cmd = [
			linker,
		] + linker_flags + [
			"-o", os.path.join(output_name),
		] + object_filenames + linker_libraries;
		compilation_errors += run_command(cmd);

		# Success
		if (compilation_errors == 0):
			return output_name;


	# Failure
	return None;



# Compile using VC
def vc_compile(output_name, target_info):
	# Settings
	descriptor = [
		"py" + target_info["python"],
		target_info["mode"],
		target_info["compiler"],
		target_info["architecture"],
	];
	if (target_info["type"] == "dll"):
		output_name = os.path.join(project_directories["bin"], "{0:s}-{1:s}{2:s}".format(output_name, "-".join(descriptor), ".dll"));
		descriptor.extend([ "dll", ]);
	else:
		output_name = os.path.join(project_directories["bin"], "{0:s}-{1:s}{2:s}".format(output_name, "-".join(descriptor), ".exe"));

	# Set the comipler
	compiler_global_info = compilers[target_info["compiler"]];
	compiler_info = compilers[target_info["compiler"]]["architectures"][target_info["architecture"]];
	compiler = compiler_info["compiler"];
	linker = compiler_info["linker"];
	cvtres = compiler_info["cvtres"];
	rc = compiler_info["rc"];
	setup_command = None;
	if ("setup" in compiler_info and target_info["mode"] in compiler_info["setup"]):
		setup_command = compiler_info["setup"][target_info["mode"]];



	# Compile flags
	compiler_flags = [
		"-I{0:s}".format(os.path.join(python_versions[target_info["python"]]["architectures"][target_info["architecture"]]["path"], "include")),
	];
	linker_flags = [
		"/LIBPATH:{0:s}".format(os.path.join(python_versions[target_info["python"]]["architectures"][target_info["architecture"]]["path"], "libs")),
	];
	linker_libraries = [
		"{0:s}.lib".format(python_versions[target_info["python"]]["architectures"][target_info["architecture"]]["library"]),
	];
	cvtres_flags = [];
	for info in [ compiler_global_info , compiler_info ]:
		if ("compiler_flags" in info and target_info["mode"] in info["compiler_flags"]):
			compiler_flags.extend(info["compiler_flags"][target_info["mode"]]);
		if ("linker_flags" in info and target_info["mode"] in info["linker_flags"]):
			linker_flags.extend(info["linker_flags"][target_info["mode"]]);
		if ("cvtres_flags" in info and target_info["mode"] in info["cvtres_flags"]):
			cvtres_flags.extend(info["cvtres_flags"][target_info["mode"]]);
		if ("linker_libraries" in info and target_info["mode"] in info["linker_libraries"]):
			linker_libraries.extend([ "{0:s}.lib".format(i) for i in info["linker_libraries"][target_info["mode"]] ]);



	# New directory
	object_dir = os.path.join(project_directories["obj"], "-".join(descriptor));
	try:
		os.makedirs(object_dir);
	except OSError:
		pass;



	# Compile object files
	object_filenames = [];
	compilation_errors = 0;
	for input in sources:
		# Setup command
		object_file = os.path.join(object_dir, "{0:s}.obj".format(os.path.splitext(input)[0]));
		pdb_file = os.path.join(object_dir, "{0:s}.pdb".format(os.path.splitext(input)[0]));
		object_filenames.append(object_file);

		# Output
		sys.stdout.write("{0:s}: compiling {1:s}\n".format(os.path.splitext(os.path.split(compiler)[1])[0], input));

		# Compile
		cmd = [
			compiler,
			"/c",
		] + compiler_flags + [
			"/Fo{0:s}".format(object_file),
			"/Fd{0:s}".format(pdb_file),
			os.path.join(project_directories["src"], input),
		];
		cmd_string = argument_list_to_command_line_string(setup_command, False) + " && " + argument_list_to_command_line_string(cmd, False);
		compilation_errors += run_command(cmd_string, shell=True);



	# Compile resources
	for input in resources:
		# Setup command
		res_file = os.path.join(object_dir, "{0:s}.res".format(os.path.splitext(input)[0]));
		coff_file = os.path.join(object_dir, "{0:s}.coff".format(os.path.splitext(input)[0]));
		object_filenames.append(coff_file);

		# Output
		sys.stdout.write("{0:s}: compiling {1:s}\n".format(os.path.splitext(os.path.split(rc)[1])[0], input));

		# Compile res
		cmd = [
			rc,
			"/fo", res_file,
			os.path.join(project_directories["res"], input),
		];
		cmd_string = argument_list_to_command_line_string(setup_command, False) + " && " + argument_list_to_command_line_string(cmd, False);
		errors = run_command(cmd_string, shell=True);
		if (errors > 0):
			compilation_errors += errors;
			continue;

		# Compile coff
		cmd = [
			cvtres,
		] + cvtres_flags + [
			"/OUT:{0:s}".format(coff_file),
			res_file,
		];
		cmd_string = argument_list_to_command_line_string(setup_command, False) + " && " + argument_list_to_command_line_string(cmd, False);
		compilation_errors += run_command(cmd_string, shell=True);



	# Link
	if (compilation_errors == 0):
		# Output
		sys.stdout.write("{0:s}: linking {1:s}\n".format(os.path.splitext(os.path.split(linker)[1])[0], os.path.splitext(os.path.split(output_name)[1])[0]));

		# Link
		cmd = [
			linker,
		] + linker_flags + [
			"/OUT:{0:s}".format(output_name),
		] + object_filenames + linker_libraries;
		cmd_string = argument_list_to_command_line_string(setup_command, False) + " && " + argument_list_to_command_line_string(cmd, False);
		compilation_errors += run_command(cmd_string, shell=True);

		# Success
		if (compilation_errors == 0):
			return output_name;


	# Failure
	return None;



# Main
def main():
	build_modes = [ "debug" , "release" ];
	build_architectures = [ "x86" , "x64" ];
	build_types = [ "application" , "dll" ];
	compiler_classes = {
		"gcc": gcc_compile,
		"vc": vc_compile,
	};
	target_info = {
		"compiler": "gcc",
		"mode": "debug",
		"architecture": "x86",
		"type": "application",
		"python": "2",
	};
	run_after = False;

	for arg in sys.argv[1 : ]:
		arg = arg.lower();

		if (arg[0 : 2] == "py" and arg[2 : ] in python_versions):
			target_info["python"] = arg[2 : ];
		elif (arg in build_modes):
			target_info["mode"] = arg;
		elif (arg in compilers):
			target_info["compiler"] = arg;
		elif (arg in build_architectures):
			target_info["architecture"] = arg;
		elif (arg in build_types):
			target_info["type"] = arg;
		elif (arg == "run"):
			run_after = True;

	# Compiler checking
	v = validate(target_info);
	if (v is not None): raise Exception(v);

	# Compile
	exe_name = compiler_classes[compilers[target_info["compiler"]]["compiler_class"]]("pyp", target_info);

	# Run
	if (run_after and exe_name is not None):
		cmd = [ exe_name, "--version" ];
		sys.stdout.write("{0:s}\n".format("=" * 80));
		p = subprocess.Popen(cmd);
		p.communicate();
		sys.stdout.write("{0:s}\n".format("=" * 80));

	# Done
	if (exe_name is None): return -1;
	return 0;



# Execute
if (__name__ == "__main__"): sys.exit(main());


