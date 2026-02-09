from conan import ConanFile

class MoxPPRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "PremakeDeps"

    def requirements(self):
        self.requires("assimp/6.0.2")
        self.requires("imgui/1.92.5")

        # self.requires("gtest/1.16.0")

    #def configure(self):
        #self.options["assimp"].shared = True
