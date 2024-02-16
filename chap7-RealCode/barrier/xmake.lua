add_rules("mode.debug")
set_toolchains("@llvm")

-- add_requires("boost")

target("brr") do
    set_kind("binary")
    add_files("./*.cxx")
	set_languages("cxx20")
	set_targetdir("./build")
end