#***************************************************************************
#*                                                                         *
#*   Copyright (c) 2015 - Przemo Firszt <przemo@firszt.eu>                 *
#*                                                                         *
#*   This program is free software; you can redistribute it and/or modify  *
#*   it under the terms of the GNU Lesser General Public License (LGPL)    *
#*   as published by the Free Software Foundation; either version 2 of     *
#*   the License, or (at your option) any later version.                   *
#*   for detail see the LICENCE text file.                                 *
#*                                                                         *
#*   This program is distributed in the hope that it will be useful,       *
#*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
#*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
#*   GNU Library General Public License for more details.                  *
#*                                                                         *
#*   You should have received a copy of the GNU Library General Public     *
#*   License along with this program; if not, write to the Free Software   *
#*   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
#*   USA                                                                   *
#*                                                                         *
#***************************************************************************


import FreeCAD
from PySide import QtCore


class FemTools(QtCore.QRunnable, QtCore.QObject):

    finished = QtCore.Signal(int)

    def __init__(self, analysis=None):
        QtCore.QRunnable.__init__(self)
        QtCore.QObject.__init__(self)

        self.known_analysis_types = ["static", "frequency"]

        if analysis:
            self.analysis = analysis
        else:
            import FemGui
            self.analysis = FemGui.getActiveAnalysis()
        if self.analysis:
            self.update_objects()
            self.set_analysis_type()
            self.set_eigenmode_parameters()
            self.base_name = ""
            self.results_present = False
            self.setup_working_dir()
            self.setup_ccx()
        else:
            raise Exception('FEM: No active analysis found!')

    ## Removes all result objects
    #  @param self The python object self
    def purge_results(self):
        for m in self.analysis.Member:
            if (m.isDerivedFrom('Fem::FemResultObject')):
                FreeCAD.ActiveDocument.removeObject(m.Name)
        self.results_present = False

    ## Resets mesh deformation
    #  @param self The python object self
    def reset_mesh_deformation(self):
        if self.mesh:
            self.mesh.ViewObject.applyDisplacement(0.0)

    ## Resets mesh color
    #  @param self The python object self
    def reset_mesh_color(self):
        if self.mesh:
            self.mesh.ViewObject.NodeColor = {}
            self.mesh.ViewObject.ElementColor = {}
            self.mesh.ViewObject.setNodeColorByScalars()

    ## Resets mesh color, deformation and removes all result objects
    #  @param self The python object self
    def reset_all(self):
        self.purge_results()
        self.reset_mesh_color()
        self.reset_mesh_deformation()

    def show_result(self, result_type="Sabs", limit=None):
        self.update_objects()
        if result_type == "None":
            self.reset_mesh_color()
            return
        if self.result_object:
            if result_type == "Sabs":
                values = self.result_object.StressValues
            elif result_type == "Uabs":
                values = self.result_object.DisplacementLengths
            else:
                match = {"U1": 0, "U2": 1, "U3": 2}
                d = zip(*self.result_object.DisplacementVectors)
                values = list(d[match[result_type]])
            self.show_color_by_scalar_with_cutoff(values, limit)

    def show_color_by_scalar_with_cutoff(self, values, limit=None):
        if limit:
            filtered_values = []
            for v in values:
                if v > limit:
                    filtered_values.append(limit)
                else:
                    filtered_values.append(v)
        else:
            filtered_values = values
        self.mesh.ViewObject.setNodeColorByScalars(self.result_object.ElementNumbers, filtered_values)

    def show_displacement(self, displacement_factor=0.0):
        self.mesh.ViewObject.setNodeDisplacementByVectors(self.result_object.ElementNumbers,
                                                          self.result_object.DisplacementVectors)
        self.mesh.ViewObject.applyDisplacement(displacement_factor)

    def update_objects(self):
        # [{'Object':material}, {}, ...]
        # [{'Object':fixed_constraints, 'NodeSupports':bool}, {}, ...]
        # [{'Object':force_constraints, 'NodeLoad':value}, {}, ...
        # [{'Object':pressure_constraints, 'xxxxxxxx':value}, {}, ...]
        # [{'Object':beam_sections, 'xxxxxxxx':value}, {}, ...]
        # [{'Object':shell_thicknesses, 'xxxxxxxx':value}, {}, ...]
        self.mesh = None
        self.material = []
        self.fixed_constraints = []
        self.force_constraints = []
        self.pressure_constraints = []
        self.beam_sections = []
        self.shell_thicknesses = []

        for m in self.analysis.Member:
            if m.isDerivedFrom("Fem::FemMeshObject"):
                self.mesh = m
            elif m.isDerivedFrom("App::MaterialObjectPython"):
                material_dict = {}
                material_dict['Object'] = m
                self.material.append(material_dict)
            elif m.isDerivedFrom("Fem::ConstraintFixed"):
                fixed_constraint_dict = {}
                fixed_constraint_dict['Object'] = m
                self.fixed_constraints.append(fixed_constraint_dict)
            elif m.isDerivedFrom("Fem::ConstraintForce"):
                force_constraint_dict = {}
                force_constraint_dict['Object'] = m
                self.force_constraints.append(force_constraint_dict)
            elif m.isDerivedFrom("Fem::ConstraintPressure"):
                PressureObjectDict = {}
                PressureObjectDict['Object'] = m
                self.pressure_constraints.append(PressureObjectDict)
            elif hasattr(m, "Proxy") and m.Proxy.Type == 'FemBeamSection':
                beam_section_dict = {}
                beam_section_dict['Object'] = m
                self.beam_sections.append(beam_section_dict)
            elif hasattr(m, "Proxy") and m.Proxy.Type == 'FemShellThickness':
                shell_thickness_dict = {}
                shell_thickness_dict['Object'] = m
                self.shell_thicknesses.append(shell_thickness_dict)

    def check_prerequisites(self):
        message = ""
        if not self.analysis:
            message += "No active Analysis\n"
        if self.analysis_type not in self.known_analysis_types:
            message += "Unknown analysis type: {}\n".format(self.analysis_type)
        if not self.mesh:
            message += "No mesh object in the Analysis\n"
        if not self.material:
            message += "No material object in the Analysis\n"
        if not self.fixed_constraints:
            message += "No fixed-constraint nodes defined in the Analysis\n"
        if self.analysis_type == "static":
            if not (self.force_constraints or self.pressure_constraints):
                message += "No force-constraint or pressure-constraint defined in the Analysis\n"
        if self.beam_sections:
            has_no_references = False
            for b in self.beam_sections:
                if len(b['Object'].References) == 0:
                    if has_no_references is True:
                        message += "More than one BeamSection has empty References list (Only one empty References list is allowed!).\n"
                    has_no_references = True
        if self.shell_thicknesses:
            has_no_references = False
            for s in self.shell_thicknesses:
                if len(s['Object'].References) == 0:
                    if has_no_references is True:
                        message += "More than one ShellThickness has empty References list (Only one empty References list is allowed!).\n"
                    has_no_references = True
        return message

    def write_inp_file(self):
        import ccxInpWriter as iw
        import sys
        self.inp_file_name = ""
        try:
            inp_writer = iw.inp_writer(self.analysis, self.mesh, self.material,
                                       self.fixed_constraints,
                                       self.force_constraints, self.pressure_constraints,
                                       self.beam_sections, self.shell_thicknesses,
                                       self.analysis_type, self.eigenmode_parameters,
                                       self.working_dir)
            self.inp_file_name = inp_writer.write_calculix_input_file()
        except:
            print "Unexpected error when writing CalculiX input file:", sys.exc_info()[0]
            raise

    def start_ccx(self):
        import multiprocessing
        import os
        import subprocess
        if self.inp_file_name != "":
            ont_backup = os.environ.get('OMP_NUM_THREADS')
            if not ont_backup:
                ont_backup = ""
            _env = os.putenv('OMP_NUM_THREADS', str(multiprocessing.cpu_count()))
            # change cwd because ccx may crash if directory has no write permission
            # there is also a limit of the length of file names so jump to the document directory
            cwd = QtCore.QDir.currentPath()
            f = QtCore.QFileInfo(self.inp_file_name)
            QtCore.QDir.setCurrent(f.path())
            p = subprocess.Popen([self.ccx_binary, "-i ", f.baseName()],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 shell=False, env=_env)
            self.ccx_stdout, self.ccx_stderr = p.communicate()
            os.putenv('OMP_NUM_THREADS', ont_backup)
            QtCore.QDir.setCurrent(cwd)
            return p.returncode
        return -1

    ## sets eigenmode parameters for CalculiX frequency analysis
    #  @param self The python object self
    #  @param number number of eigenmodes that wll be calculated, default 10
    #  @param limit_low lower value of requested eigenfrequency range, default 0.0
    #  @param limit_high higher value of requested eigenfrequency range, default 1000000.0
    def set_eigenmode_parameters(self, number=10, limit_low=0.0, limit_high=1000000.0):
        self.eigenmode_parameters = (number, limit_low, limit_high)

    def set_base_name(self, base_name=None):
        if base_name is None:
            self.base_name = ""
        else:
            self.base_name = base_name
        # Update inp file name
        self.set_inp_file_name()

    ## sets inp file name that is used to determine location and name of frd result file.
    # Normally inp file name is set set by write_inp_file
    # Can be used to read mock calculations file
    def set_inp_file_name(self, inp_file_name=None):
        if inp_file_name is not None:
            self.inp_file_name = inp_file_name
        else:
            self.inp_file_name = self.working_dir + '/' + self.base_name + '.inp'

    def set_analysis_type(self, analysis_type=None):
        if analysis_type is None:
            self.analysis_type = "static"
        else:
            self.analysis_type = analysis_type

    ## Sets working dir for ccx execution. Called with no working_dir uses WorkingDir for FEM preferences
    #  @param self The python object self
    #  @working_dir directory to be used for .inp file and ccx execution
    def setup_working_dir(self, working_dir=None):
        if working_dir is None:
            self.fem_prefs = FreeCAD.ParamGet("User parameter:BaseApp/Preferences/Mod/Fem")
            self.working_dir = self.fem_prefs.GetString("WorkingDir", "/tmp")
        else:
            self.working_dir = working_dir
        # Update inp file name
        self.set_inp_file_name()

    def setup_ccx(self, ccx_binary=None):
        if not ccx_binary:
            self.fem_prefs = FreeCAD.ParamGet("User parameter:BaseApp/Preferences/Mod/Fem")
            ccx_binary = self.fem_prefs.GetString("ccxBinaryPath", "")
        if not ccx_binary:
            from platform import system
            if system() == "Linux":
                ccx_binary = "ccx"
            elif system() == "Windows":
                ccx_binary = FreeCAD.getHomePath() + "bin/ccx.exe"
            else:
                ccx_binary = "ccx"
        self.ccx_binary = ccx_binary

    ## Load results of ccx calculiations from .frd file.
    #  @param self The python object self
    def load_results(self):
        import ccxFrdReader
        import os
        self.results_present = False
        frd_result_file = os.path.splitext(self.inp_file_name)[0] + '.frd'
        if os.path.isfile(frd_result_file):
            ccxFrdReader.importFrd(frd_result_file, self.analysis)
            for m in self.analysis.Member:
                if m.isDerivedFrom("Fem::FemResultObject"):
                    self.result_object = m
            if self.result_object is not None:
                self.results_present = True
        else:
            raise Exception('FEM: No results found at {}!'.format(frd_result_file))

    def use_results(self, results_name=None):
        for m in self.analysis.Member:
            if m.isDerivedFrom("Fem::FemResultObject") and m.Name == results_name:
                self.result_object = m
                break
        if not self.result_object:
            raise ("{} doesn't exist".format(results_name))

    def run(self):
        ret_code = 0
        message = self.check_prerequisites()
        if not message:
            self.write_inp_file()
            from FreeCAD import Base
            progress_bar = Base.ProgressIndicator()
            progress_bar.start("Running CalculiX ccx...", 0)
            ret_code = self.start_ccx()
            self.finished.emit(ret_code)
            progress_bar.stop()
        else:
            print "Running analysis failed! " + message
        if ret_code or self.ccx_stderr:
            print "Analysis failed with exit code {}".format(ret_code)
            print "--------start of stderr-------"
            print self.ccx_stderr
            print "--------end of stderr---------"
            print "--------start of stdout-------"
            print self.ccx_stdout
            print "--------end of stdout---------"

    ## returns minimum, average and maximum value for provided result type
    #  @param self The python object self
    #  @result_type Type of FEM result, allowed U1, U2, U3, Uabs, Sabs and None
    def get_stats(self, result_type):
        stats = (0.0, 0.0, 0.0)
        for m in self.analysis.Member:
            if m.isDerivedFrom("Fem::FemResultObject"):
                match = {"U1": (m.Stats[0], m.Stats[1], m.Stats[2]),
                         "U2": (m.Stats[3], m.Stats[4], m.Stats[5]),
                         "U3": (m.Stats[6], m.Stats[7], m.Stats[8]),
                         "Uabs": (m.Stats[9], m.Stats[10], m.Stats[11]),
                         "Sabs": (m.Stats[12], m.Stats[13], m.Stats[14]),
                         "None": (0.0, 0.0, 0.0)}
                stats = match[result_type]
        return stats
