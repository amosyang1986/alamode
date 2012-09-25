#include "fcs_phonon.h"
#include "system.h"
#include "memory.h"
#include "error.h"
#include "phonons.h"
#include "../alm_c++/constants.h"
#include <string>
#include <iomanip>
#include <fstream>
#include <boost/lexical_cast.hpp>
#include <algorithm>

using namespace PHON_NS;

Fcs_phonon::Fcs_phonon(PHON *phon): Pointers(phon) {
    maxorder = 2;
}

Fcs_phonon::~Fcs_phonon(){}

void Fcs_phonon::setup(std::string mode)
{
    unsigned int nat = system->nat;
    unsigned int natmin = system->natmin;
    memory->allocate(fc2, natmin, nat, 3, 3);

    unsigned int i, j, icrd, jcrd;
    for (i = 0; i < natmin; ++i){
        for (j = 0; j < nat; ++j){
            for (icrd = 0; icrd < 3; ++icrd){
                for (jcrd = 0; jcrd < 3; ++jcrd){
                    fc2[i][j][icrd][jcrd] = 0.0;
                }
            }
        }
    }

    load_fc2();

    if (mode == "boltzmann"){
        memory->allocate(force_constant, 2);
        load_fcs();
        for (i = 0; i < maxorder; ++i){
            std::cout << "Number of non-zero IFCs for " << i + 2 << " order: ";
            std::cout << force_constant[i].size() << std::endl;
        }
    }
}

void Fcs_phonon::load_fc2()
{
    unsigned int i, icrd, jcrd;
    unsigned int iat, jat;
    unsigned int inatmin;

    unsigned int len1, len2;

    std::string str_tmp, str1, str2;
    std::ifstream ifs_fcs;
    unsigned int nfc2;
    double fc2_tmp;

    ifs_fcs.open(file_fcs.c_str(), std::ios::in);

    while(!ifs_fcs.eof())
    {
        std::getline(ifs_fcs, str_tmp);
        if(str_tmp == "#FCS_HARMONIC"){
            ifs_fcs >> nfc2;
            ifs_fcs.ignore();
            std::getline(ifs_fcs, str_tmp);

            for (i = 0; i < nfc2; ++i){

                ifs_fcs >> fc2_tmp;

                ifs_fcs >> str1 >> str2;
                len1 = str1.size();
                len2 = str2.size();

                iat = atoi(str1.substr(0, len1 - 1).c_str()) - 1;
                jat = atoi(str2.substr(0, len2 - 1).c_str()) - 1;
                icrd = coordinate_index(str1[len1 - 1]);
                jcrd = coordinate_index(str2[len2 - 1]);

                inatmin = system->map_s2p[iat].atom_num;
                fc2[inatmin][jat][icrd][jcrd] = fc2_tmp;
            }
        }
    }
    ifs_fcs.close();
}

void Fcs_phonon::load_fcs()
{
    unsigned int i, iorder;
    unsigned int ifcs, nfcs;

    double val;

    unsigned int atmn, xyz;
    std::string str_int;
    unsigned int len;
    unsigned int *ind;

    Triplet tri_tmp;
    std::vector<unsigned int> ivec;
    std::vector<Triplet> tri_vec;

    bool flag_found;
    std::ifstream ifs_fcs;
    std::string str_tmp;
    std::string *flag_str;

    std::cout << "Reading harmonic and anharmonic force constants from the info file..." << std::endl;

    memory->allocate(flag_str, maxorder);
    ifs_fcs.open(file_fcs.c_str(), std::ios::in);

    memory->allocate(ind, maxorder + 1);

    for (iorder = 0; iorder < maxorder; ++iorder){

        ifs_fcs.clear();
        ifs_fcs.seekg(0, std::ios_base::beg);

        if (iorder == 0) {
            flag_str[iorder] = "#FCS_HARMONIC";
        } else {
            flag_str[iorder] = "#FCS_ANHARM" + boost::lexical_cast<std::string>(iorder + 2);
        }

        flag_found = false;

        while(!ifs_fcs.eof() && !flag_found)
        {
            std::getline(ifs_fcs, str_tmp);
            if (str_tmp == flag_str[iorder]) {

                flag_found = true;
                ifs_fcs >> nfcs;
                ifs_fcs.ignore();
                std::getline(ifs_fcs,str_tmp);

                for (ifcs = 0; ifcs < nfcs; ++ifcs){

                    ifs_fcs >> val;
                    ivec.clear();

                    for (i = 0; i < iorder + 2; ++i){
                        ifs_fcs >> str_int;
                        len = str_int.size();
                        atmn = atoi(str_int.substr(0, len - 1).c_str()) - 1;
                        xyz = coordinate_index(str_int[len - 1]);
                        ivec.push_back(3 * atmn + xyz);
                    }

                    if (std::abs(val) > eps) {
                        do {
                            tri_vec.clear();

                            for (i = 0; i < iorder + 2; ++i){
                                tri_tmp.atom = system->map_s2p[ivec[i] / 3].atom_num;
                                tri_tmp.cell = system->map_s2p[ivec[i] / 3].tran_num;
                                tri_tmp.xyz  = ivec[i] % 3;

                                tri_vec.push_back(tri_tmp);
                            }

                            force_constant[iorder].push_back(FcsClass(val, tri_vec));
                       
                        } while (std::next_permutation(ivec.begin() + 1, ivec.end()));            
                    }
                }
            }
        }

        if (!flag_found){
            str_tmp = flag_str[iorder] + " flag not found in the info file";
            error->exit("load_fcs", str_tmp.c_str());
        }
    }

    ifs_fcs.close();

    memory->deallocate(ind);
    std::cout << "Done !" << std::endl;
/*
    for (std::vector<FcsClass>::iterator p = force_constant[1].begin(); p != force_constant[1].end(); ++p)
    {
        std::cout << "$";
        std::cout << std::setw(15) << (*p).fcs_val;
        std::cout << std::setw(5) << (*p).elems[0].cell;
        std::cout << std::setw(5) << (*p).elems[0].atom;
        std::cout << std::setw(5) << (*p).elems[0].xyz;
        std::cout << std::setw(5) << (*p).elems[1].cell;
        std::cout << std::setw(5) << (*p).elems[1].atom;
        std::cout << std::setw(5) << (*p).elems[1].xyz;
        std::cout << std::setw(5) << (*p).elems[2].cell;
        std::cout << std::setw(5) << (*p).elems[2].atom;
        std::cout << std::setw(5) << (*p).elems[2].xyz;
        std::cout << std::endl;
    }
*/
}

unsigned int Fcs_phonon::coordinate_index(const char char_coord)
{
    unsigned int m;
    if(char_coord == 'x'){
        m = 0;
    } else if (char_coord == 'y'){
        m = 1;
    } else if (char_coord == 'z'){
        m = 2;
    } else {
        error->exit("coordinate_index", "invalid char_coord", char_coord);
    }
    return m;
}
