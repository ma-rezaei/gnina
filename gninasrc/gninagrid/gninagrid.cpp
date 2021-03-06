/*
 * gninagrid.cpp
 *
 *  Created on: Nov 4, 2015
 *      Author: dkoes
 *
 * Output a voxelation of a provided receptor and ligand.
 * For every (heavy) atom type and grid point compute an occupancy value.
 */

#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <boost/program_options.hpp>
#include <boost/multi_array.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <openbabel/oberror.h>

#include "atom_type.h"
#include "box.h"
#include "molgetter.h"

#include "gridoptions.h"
#include "nngridder.h"

using namespace std;
using namespace boost;

//parse commandline options using boost::program_options and put the values in opts
//return true if successfull and ready to compute
//will exit on error
static bool parse_options(int argc, char *argv[], gridoptions& o)
{
	using namespace boost::program_options;
	positional_options_description positional; // remains empty

	options_description inputs("Input");
	inputs.add_options()
	("receptor,r", value<std::string>(&o.receptorfile)->required(),
			"receptor file")
	("ligand,l", value<std::string>(&o.ligandfile)->required(), "ligand(s)")
	("grid,g", value<std::vector<std::string> >(&o.usergrids)->multitoken(), "grid(s) dx format");

	options_description outputs("Output");
	outputs.add_options()
	("out,o", value<std::string>(&o.outname)->required(),
			"output file name base, combined map of both lig and receptor")
	("map", bool_switch(&o.outmap),
			"output AD4 map files (for debugging, out is base name)")
	("dx", bool_switch(&o.outdx),
      "output DX map files (for debugging, out is base name)");

	options_description options("Options");
	options.add_options()
	("dimension", value<double>(&o.dim), "Cubic grid dimension (Angstroms)")
	("resolution", value<double>(&o.res), "Cubic grid resolution (Angstroms)")
	("binary_occupancy", bool_switch(&o.binary),
			"Output binary occupancies (still as floats)")
	("random_rotation", bool_switch(&o.randrotate),
			"Apply random rotation to input")
	("random_translation", value<fl>(&o.randtranslate),
			"Apply random translation to input up to specified distance")
	("random_seed", value<int>(&o.seed), "Random seed to use")
	("recmap", value<string>(&o.recmap), "Atom type mapping for receptor atoms")
	("ligmap", value<string>(&o.ligmap), "Atom type mapping for ligand atoms")
	("separate", bool_switch(&o.separate), "Output separate rec and lig files.")
	("gpu", bool_switch(&o.gpu), "Use GPU to compute grids");

	options_description info("Information (optional)");
	info.add_options()
	("help", bool_switch(&o.help), "display usage summary")
	("version", bool_switch(&o.version), "display program version")
	("time", bool_switch(&o.timeit), "display time to grid")
	("verbosity", value<int>(&o.verbosity)->default_value(1),
			"Adjust the verbosity of the output, default: 1");
	options_description desc;
	desc.add(inputs).add(options).add(outputs).add(info);
	variables_map vm;
	try
	{
		store(
				command_line_parser(argc, argv).options(desc)
						.style(
						command_line_style::default_style
								^ command_line_style::allow_guessing)
						.positional(positional).run(), vm);

		//process informational
		if (o.help)
		{
			cout << desc << '\n';
			return false;
		}
		if (o.version)
		{
			cout << "gnina " __DATE__ << '\n';
			return false;
		}

		notify(vm);
	} catch (boost::program_options::error& e)
	{
		std::cerr << "Command line parse error: " << e.what() << '\n'
				<< "\nCorrect usage:\n" << desc << '\n';
		exit(-1);
	}

	return true;
}

int main(int argc, char *argv[])
{
	OpenBabel::obErrorLog.StopLogging();
	try
	{
		//setup commandline options
		gridoptions opt;
		if (!parse_options(argc, argv, opt))
			exit(0);

		srand(opt.seed);

		//setup receptor grid
		NNMolsGridder gridder(opt);

		if(opt.separate)
		{
			string outname = opt.outname + "." + gridder.getParamString(true,false) + ".binmap";
			ofstream binout(outname.c_str());
			if (!binout)
			{
				cerr << "Could not open " << outname << "\n";
				exit(-1);
			}
			gridder.outputBIN(binout,true,false);
		}

		//for each ligand..
		unsigned ligcnt = 0;
		while (gridder.readMolecule(opt.timeit))
		{ //computes ligand grid
			//and output
			string base = opt.outname + "_" + lexical_cast<string>(ligcnt);

			if (opt.outmap)
			{
				gridder.outputMAP(base);
			}
			else if(opt.outdx)
			{
			  gridder.outputDX(base);
			}
			else if(opt.separate)
			{
				string outname = base + "." + gridder.getParamString(false, true) + ".binmap";
				ofstream binout(outname.c_str());
				if (!binout)
				{
					cerr << "Could not open " << outname << "\n";
					exit(-1);
				}
				gridder.outputBIN(binout,false,true);
			}
			else
			{
				string outname = base + "." + gridder.getParamString(true, true) + ".binmap";
				ofstream binout(outname.c_str());
				if (!binout)
				{
					cerr << "Could not open " << outname << "\n";
					exit(-1);
				}
				gridder.outputBIN(binout);
			}
			ligcnt++;
		}

	} catch (file_error& e)
	{
		std::cerr << "\n\nError: could not open \"" << e.name.string()
				<< "\" for " << (e.in ? "reading" : "writing") << ".\n";
		return -1;
	}
}
