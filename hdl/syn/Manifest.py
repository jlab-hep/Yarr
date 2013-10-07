target = "xilinx"
action = "synthesis"

modules = {"local" : "../"}

syn_device = "xc6slx45t"
syn_grade = "-3"
syn_package = "fgg484"
syn_top = "yarr"
syn_project = "yarr_spec.xise"

files = ["../yarr_spec.ucf",
         "../top_yarr_spec.vhd"]

fetchto = "../ip_cores"
