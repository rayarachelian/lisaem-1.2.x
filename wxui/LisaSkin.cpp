/**************************************************************************************\
*                                                                                      *
*              The Lisa Emulator Project  V1.2.7      DEV 2019.09.09                   *
*                             http://lisaem.sunder.net                                 *
*                                                                                      *
*                  Copyright (C) 1998, 2019 Ray A. Arachelian                          *
*                                All Rights Reserved                                   *
*                                                                                      *
*           This program is free software; you can redistribute it and/or              *
*           modify it under the terms of the GNU General Public License                *
*           as published by the Free Software Foundation; either version 2             *
*           of the License, or (at your option) any later version.                     *
*                                                                                      *
*           This program is distributed in the hope that it will be useful,            *
*           but WITHOUT ANY WARRANTY; without even the implied warranty of             *
*           MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
*           GNU General Public License for more details.                               *
*                                                                                      *
*           You should have received a copy of the GNU General Public License          *
*           along with this program;  if not, write to the Free Software               *
*           Foundation, Inc., 59 Temple Place #330, Boston, MA 02111-1307, USA.        *
*                                                                                      *
*                   or visit: http://www.gnu.org/licenses/gpl.html                     *
*                                                                                      *
*                                                                                      *
\**************************************************************************************/

#include <wx/defs.h>
#include <wx/string.h>
#include <wx/config.h>
#include <wx/fileconf.h>
#include <wx/stdpaths.h>

#include <machine.h>
#include <LisaSkin.h>

/*
void LisaSkin::LisaSkin(void)
{
     power_button_tl_x        = 1143;
     power_button_tl_y        = 836;
     power_button_br_x        = 1185;
     power_button_br_y        = 876;
     power_frame_left         = 0;  // 4th segment of skin
     power_frame_top          = 736; 

     // floppy top left x,y (bottom right is calculated from the image)
     // floppy 1 only exists on Lisa 1
     floppy1_tl_x             = 1099;
     floppy1_tl_y             = 481;

     // floppy 2 is the only floppy on a Lisa 2 or XL
     floppy2_tl_x             = 1099;
     floppy2_tl_y             = 481;

     screen_origin_x          = 177; // was 205,175 // was 230,195 //was 140 not hidpi_scaled
     screen_origin_y          = 153;

     default_screen_origin_x  = 177; // was 205,175
     default_screen_origin_y  = 153;

     width_size               = 1484; 
     height_size              = 1026;

     lisaface0name            = "lisaface0.png";
     lisaface1name            = "lisaface1.png";
     lisaface2name            = "lisaface2.png";
     lisaface3name            = "lisaface3.png";

        // reserve for future
     floppy1anim0             = "";
     floppy1anim1             = "";
     floppy1anim2             = "";
     floppy1anim3             = "";
     floppy1animN             = "";

     floppy2anim0             = "floppy0.png";
     floppy2anim1             = "floppy1.png";
     floppy2anim2             = "floppy2.png";
     floppy2anim3             = "floppy3.png";
     floppy2animN             = "floppyN.png";

     powerOn                  = "power_on.png";
     powerOff                 = "power_off.png";

     floppy_eject             = "floppy_eject.wav";
     floppy_insert            = "floppy_insert_sound.wav";
     floppy_motor1            = "floppy_motor1.wav";
     floppy_motor2            = "floppy_motor2.wav";
     lisa_power_switch01      = "lisa_power_switch01.wav";
     lisa_power_switch02      = "lisa_power_switch02.wav";
     poweroffclk              = "poweroffclk.wav";
}
*/

void LisaSkin::Load(wxFileConfig* config)
{
     power_button_tl_x       = (int)config->Read(_T("/power/power_button/tl_x"),(long)   1143);
     power_button_tl_y       = (int)config->Read(_T("/power/power_button/tl_y"),(long)    836);
     power_button_br_x       = (int)config->Read(_T("/power/power_button/br_y"),(long)   1185);
     power_button_br_y       = (int)config->Read(_T("/power/power_button/br_y"),(long)    876);

     power_frame_left        = (int)config->Read(_T("/power/power_frame_left"), (long)      0); // 4th segment of skin
     power_frame_top         = (int)config->Read(_T("/power/power_frame_top"),  (long)    736);

        // floppy top left x,y (bottom right is calculated from the image)
        // floppy 1 only exists on Lisa 1
     floppy1_tl_x            = (int)config->Read(_T("/floppy/1/x"),             (long)   1099);
     floppy1_tl_y            = (int)config->Read(_T("/floppy/1/y"),             (long)    481);

        // floppy 2 is the only floppy on a Lisa 2 or XL
     floppy2_tl_x            = (int)config->Read(_T("/floppy/2/x"),             (long)   1099);
     floppy2_tl_y            = (int)config->Read(_T("/floppy/2/x"),             (long)    481);

     screen_origin_x         = (int)config->Read(_T("/crt/origin/x"),           (long)    145); // was 177,153 // was 205,175 // was 230,195 //was 140 not hidpi_scaled
     screen_origin_y         = (int)config->Read(_T("/crt/origin/y"),           (long)    128);
     default_screen_origin_x = (int)config->Read(_T("/crt/origin/default/x"),   (long)    145); // was 205,175
     default_screen_origin_y = (int)config->Read(_T("/crt/origin/default/y"),   (long)    128);

     width_size              = (int)config->Read(_T("/skin/width"),             (long)   1484);
     height_size             = (int)config->Read(_T("/skin/height"),            (long)   1026);

     lisaface0name           = config->Read(_T("/lisaface0"),                   _T("lisaface0.png") );
     lisaface1name           = config->Read(_T("/lisaface1"),                   _T("lisaface1.png") );
     lisaface2name           = config->Read(_T("/lisaface2"),                   _T("lisaface2.png") );
     lisaface3name           = config->Read(_T("/lisaface3"),                   _T("lisaface3.png") );

    // reserve for future
     floppy1anim0            = config->Read(_T("/floppy1/anim0"),               _T("") );
     floppy1anim1            = config->Read(_T("/floppy1/anim1"),               _T("") );
     floppy1anim2            = config->Read(_T("/floppy1/anim2"),               _T("") );
     floppy1anim3            = config->Read(_T("/floppy1/anim3"),               _T("") );
     floppy1animN            = config->Read(_T("/floppy1/animN"),               _T("") );

     floppy2anim0            = config->Read(_T("/floppy2/anim0"),               _T("floppy0.png") );
     floppy2anim1            = config->Read(_T("/floppy2/anim1"),               _T("floppy1.png") );
     floppy2anim2            = config->Read(_T("/floppy2/anim2"),               _T("floppy2.png") );
     floppy2anim3            = config->Read(_T("/floppy2/anim3"),               _T("floppy3.png") );
     floppy2animN            = config->Read(_T("/floppy2/animN"),               _T("floppyN.png") );

     powerOn                 = config->Read(_T("/power/poweron-skin"),          _T("power_on.png") ); 
     powerOff                = config->Read(_T("/power/poweroff-skin"),         _T("power_off.png") );

     floppy_eject            = config->Read(_T("/sounds/floppy_eject"),         _T("floppy_eject.wav") );
     floppy_insert           = config->Read(_T("/sounds/floppy_insert"),        _T("floppy_insert_sound.wav") );
     floppy_motor1           = config->Read(_T("/sounds/floppy_motor1"),        _T("floppy_motor1.wav") );
     floppy_motor2           = config->Read(_T("/sounds/floppy_motor2"),        _T("floppy_motor2.wav") );
     lisa_power_switch01     = config->Read(_T("/sounds/powerswitch_push"),     _T("lisa_power_switch01.wav") );
     lisa_power_switch02     = config->Read(_T("/sounds/powerswitch_release"),  _T("lisa_power_switch02.wav") );
     poweroffclk             = config->Read(_T("/sounds/poweroff"),             _T("poweroffclk.wav") );
}


void LisaSkin::Save(wxFileConfig* config)
{

     config->Write(_T("/power/power_button/tl_x"),power_button_tl_x);
     config->Write(_T("/power/power_button/tl_y"),power_button_tl_y);
     config->Write(_T("/power/power_button/br_x"),power_button_br_x);
     config->Write(_T("/power/power_button/br_y"),power_button_br_y);
     
     config->Write(_T("/power/power_frame_left"), power_frame_left); // 4th segment of skin
     config->Write(_T("/power/power_frame_top"),  power_frame_top);

     config->Write(_T("/floppy/1/x"),floppy1_tl_x);
     config->Write(_T("/floppy/1/y"),floppy1_tl_y);

     config->Write(_T("/floppy/2/x"),floppy2_tl_x);
     config->Write(_T("/floppy/2/x"),floppy2_tl_y);
     config->Write(_T("/crt/origin/x"),screen_origin_x); // was 205,175 // was 230,195 //was 140 not hidpi_scaled
     config->Write(_T("/crt/origin/y"),screen_origin_y);
     config->Write(_T("/crt/origin/default/x"),default_screen_origin_x); // was 205,175
     config->Write(_T("/crt/origin/default/y"),default_screen_origin_y);
     config->Write(_T("/skin/width"),width_size);
     config->Write(_T("/skin/height"),height_size);

     config->Write(_T("/lisaface0"),                  lisaface0name );
     config->Write(_T("/lisaface1"),                  lisaface1name );
     config->Write(_T("/lisaface2"),                  lisaface2name );
     config->Write(_T("/lisaface3"),                  lisaface3name );

     config->Write(_T("/floppy1/anim0"),              floppy1anim0  );
     config->Write(_T("/floppy1/anim1"),              floppy1anim1  );
     config->Write(_T("/floppy1/anim2"),              floppy1anim2  );
     config->Write(_T("/floppy1/anim3"),              floppy1anim3  );
     config->Write(_T("/floppy1/animN"),              floppy1animN  );

     config->Write(_T("/floppy2/anim0"),              floppy2anim0  );
     config->Write(_T("/floppy2/anim1"),              floppy2anim1  );
     config->Write(_T("/floppy2/anim2"),              floppy2anim2  );
     config->Write(_T("/floppy2/anim3"),              floppy2anim3  );
     config->Write(_T("/floppy2/animN"),              floppy2animN  );

     config->Write(_T("/power/poweron-skin"),         powerOn ); 
     config->Write(_T("/power/poweroff-skin"),        powerOff );
     config->Write(_T("/sounds/floppy_eject"),        floppy_eject );
     config->Write(_T("/sounds/floppy_insert"),       floppy_insert );
     config->Write(_T("/sounds/floppy_motor1"),       floppy_motor1 );
     config->Write(_T("/sounds/floppy_motor2"),       floppy_motor2 );
     config->Write(_T("/sounds/powerswitch_push"),    lisa_power_switch01 );
     config->Write(_T("/sounds/powerswitch_release"), lisa_power_switch02 );
     config->Write(_T("/sounds/poweroff"),            poweroffclk );

     config->Flush();
  }