//============================================================================
// Name        : urg-handler.cpp
// Author      : tyamada
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <stdio.h>
#include <sys/time.h>

#include <scip2awd.h>

#include <ssm.hpp>
#include <ssmtype/spur-odometry.h>
#include "ssm-laser.hpp"

#include "gnd-shutoff.hpp"
#include "gnd-time.hpp"
#include "gnd-configuration.hpp"
#include "gnd-util.h"
#include "gnd-matrix-coordinate.hpp"
#include "gnd-matrix-base.hpp"

#include "urg-proxy.hpp"
#include "urg-proxy-opt.hpp"
#include "urg-proxy-device-conf.hpp"

struct Scanning {
	SSMScanPoint2D ssm;
	URGProxy::ScanningProperty prop;
	S2Param_t param;
};
typedef struct Scanning Scanning;

/**
 * @brief callback function
 * @param *aScan Scan data
 * @param *aData User's data
 * @retval 1 success to analyze scan data.
 * @retval 0 failed to analyze scan data and stop thread of duall buffer.
 */
int callback( S2Scan_t *s, void *u)
{
	struct timeval tv;
	double *t = (double*)u;

	gettimeofday(&tv, 0);
	*t = gnd_timeval2time(&tv);

	return 1;
}


int main(int argc, char* argv[]) {
	SSMScanPoint2D scan_ssm;						// laser scanner reading ssm
	URGProxy::ScanningProperty scan_prop;			// scan property
	URGProxy::ssm_property ssm_prop;			// laser scanner configuration
	URGProxy::device_configuration conf_device;

	URGProxy::TimeAdjust tmadj;
	URGProxy::TimeAdjustProperty tmadj_prop;
	double recv_time;

	S2Port* port = 0;
	S2Sdd_t buffer;
	speed_t bitrate = B0;
	S2Ver_t version;
	S2Param_t param;

	gnd::urg_proxy::configuration proc_conf;
	gnd::urg_proxy::options proc_opt(&proc_conf);

	{ // ---> gnd-configuration debug
		FILE *fp;
		fp = fopen("gnd-conf.log", "w");
		gnd::Conf::debug_set_level(0);
		gnd::Conf::debug_set_fstream(fp);
	} // <--- gnd-configuration debug

	{ // ---> urg-proxy debug
		FILE *fp;
		fp = fopen("urg-proxy.log", "w");
		URGProxy::debug_set_level(2);
		URGProxy::debug_set_fstream(fp);
	} // <--- urg-proxy debug


	{ // ---> initialize
		int ret;
		int phase = 1;
		gnd::Conf::FileStream fst_conf;			// device configuration file

		// ---> read process options
		if( (ret = proc_opt.get_option(argc, argv)) != 0 ) {
			return ret;
		} // <--- read process options


		{ // ---> show task
			::fprintf(stderr, "========== Initialize ==========\n");
			::fprintf(stderr, " %d. read device configuration file\n", phase++);
			::fprintf(stderr, " %d. open device port\n", phase++);
			::fprintf(stderr, " %d. initailize SSM\n", phase++);
			::fprintf(stderr, " %d. craete ssm-data \"\x1b[4m%s\x1b[0m\"\n", phase++, SSM_NAME_SCAN_POINT_2D);
			::fprintf(stderr, "\n");
		} // <--- show task



		{ // ---> allocate SIGINT to shut-off
			::proc_shutoff_clear();
			::proc_shutoff_alloc_signal(SIGINT);
		} // <--- allocate SIGINT to shut-off



		// ---> read device configuration
		if( !::is_proc_shutoff() && *proc_conf.dev_conf.value ){
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => read device configuration file\n");
			if( fst_conf.read(proc_conf.dev_conf.value) < 0 ){
				::proc_shutoff();
				::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to read configuration file \"\x1b[4m%s\x1b[0m\"\n", proc_conf.dev_conf.value);
			}
			else {
				::fprintf(stderr, " ...\x1b[1mOK\x1b[0m\n");
			}
		} // <--- read device configuration



		// ---> open device port
		if( !::is_proc_shutoff() ){
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => open device port \"%s\"\n", proc_conf.dev_port.value);

			// initialize
			::S2Sdd_Init( &buffer );
			::S2Sdd_setCallback(&buffer, callback, &recv_time);

			// port open
			if( !(port = ::Scip2_Open(proc_conf.dev_port.value, bitrate) ) ){
				::proc_shutoff();
				::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: faile to open port \"\x1b[4m%s\x1b[0m\"\n", "/dev/ttyACM0");
			}
			else {
				gnd::Conf::Configuration *pconf;

				// get device version infomation
				::Scip2CMD_VV(port, &version);
				URGProxy::show_version(stderr, &version);

				// get device parameter infomation
				::Scip2CMD_PP(port, &param);


				scan_prop.step.min = param.step_min;
				scan_prop.step.max = param.step_max;

				// ---> find device configuration
				if( ( pconf = fst_conf.child_find(version.serialno, 0) ) == 0) {
					::fprintf(stdout, "     \x1b[1mWarnning\x1b[0m:missing device configuration\n");
				}
				else {
					// get configuration parameter
					URGProxy::get_configuration_parameter(pconf, &conf_device);

					// ssm-id
					ssm_prop.id = conf_device.id.value;
					::fprintf(stdout, " ... ssm-id is %d\n", ssm_prop.id);

					{ // ---> coordinate matrix
						gnd::matrix::coordinate_converter(&ssm_prop.coord,
								conf_device.position.value[0], conf_device.position.value[1], conf_device.position.value[2],
								conf_device.orient.value[0], conf_device.orient.value[1], conf_device.orient.value[2],
								conf_device.upside.value[0], conf_device.upside.value[1], conf_device.upside.value[2]);
						::fprintf(stderr, " ... coordinate matrix is following ...\n");
					} // <--- coordinate matrix

					// ---> time adjust
					tmadj_prop.min_poll = conf_device.timeadjust.value[0] > conf_device.timeadjust.value[1] ? 0: conf_device.timeadjust.value[0];
					tmadj_prop.max_poll = conf_device.timeadjust.value[1];
					if( tmadj_prop.min_poll > 0 ){
						struct timeval htime;
						unsigned long dtime;

						::fprintf(stdout, " ... time adjust initialize.\n");
						if( !::Scip2CMD_TM_GetSyncTime(port, &dtime, &htime) ) {
							::proc_shutoff();
						}
						else {
							URGProxy::timeadjust_initialize( &tmadj_prop, &tmadj, &dtime, &htime );
						}
					} // <--- time adjust

					{ // ---> scanning property
						// step min, max
						scan_prop.step.min = param.step_front
								+ ((double) conf_device.angle_range.value[0] / (360.0 / param.step_resolution));
						scan_prop.step.min = scan_prop.step.min > param.step_min ? scan_prop.step.min : param.step_min;
						scan_prop.step.max = param.step_front
								+ (conf_device.angle_range.value[1] / (360.0 / param.step_resolution));
						scan_prop.step.max = scan_prop.step.max < param.step_max ? scan_prop.step.max : param.step_max;
						::fprintf(stderr, " ... scanning range is from %d to %d\n", scan_prop.step.min, scan_prop.step.max);
						conf_device.angle_range.value[0] = (double) 360.0 * (scan_prop.step.min - param.step_front) / param.step_resolution;
						conf_device.angle_range.value[1] = (double) 360.0 * (scan_prop.step.max - param.step_front) / param.step_resolution;
						// reflect
						scan_prop.intensity = conf_device.reflect.value;
						::fprintf(stderr, " ... get intensity \"%s\"\n", scan_prop.intensity ? "on" : "off");
					} // <--- scanning property


					{ // ---> scan reading property
						scan_prop.step_front = param.step_front;
						scan_prop.angle_resolution = 2 * M_PI / param.step_resolution;
					} // <--- scan reading property
				}
				// ---> find device configuration

				{ // ---> ssm-property

					scan_ssm.property.numPoints = ( scan_prop.step.max - scan_prop.step.min + 1 );
					scan_ssm.property.distMin = param.dist_min / 1000.0;// mm -> m
					scan_ssm.property.distMax = param.dist_max / 1000.0;// mm -> m
					scan_ssm.property.angMin =  ( scan_prop.step.min - param.step_front ) * param.revolution;
					scan_ssm.property.angMax =  ( scan_prop.step.max - param.step_front ) * param.revolution;
					scan_ssm.property.angResolution =  param.revolution;
					scan_ssm.property.cycle = 1.0/ ( (double)(param.revolution) / 60.0 );
					gnd::matrix::copy( &scan_ssm.property.coordm, &ssm_prop.coord);

					strncpy( scan_ssm.property.sensorInfo.firmware, version.firmware, ssm::ScanPoint2DProperty::LENGTH_MAX );
					strncpy( scan_ssm.property.sensorInfo.product, version.product, ssm::ScanPoint2DProperty::LENGTH_MAX );
					strncpy( scan_ssm.property.sensorInfo.protocol, version.protocol, ssm::ScanPoint2DProperty::LENGTH_MAX );
					strncpy( scan_ssm.property.sensorInfo.id, version.serialno, ssm::ScanPoint2DProperty::LENGTH_MAX );
					strncpy( scan_ssm.property.sensorInfo.vendor, version.vender, ssm::ScanPoint2DProperty::LENGTH_MAX );

					scan_ssm.data.alloc( scan_ssm.property.numPoints );

				} // <--- ssm-property
				::fprintf(stderr, " ...\x1b[1mOK\x1b[0m\n");
			}
		} // <--- open device port



		// ---> initialize ssm
		if(!::is_proc_shutoff()){
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => Initailize SSM\n");
			if( !::initSSM() ) {
				::proc_shutoff();
				::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to initialize \x1b[4mssm\x1b[0m\n");
			}
			else {
				::fprintf(stderr, " ...\x1b[1mOK\x1b[0m\n");
			}
		} // <--- initialize ssm


		// ---> create sokuiki-data ssm
		if( !::is_proc_shutoff() ){
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => Create ssm-data \"\x1b[4m%s\x1b[0m\"\n", SSM_NAME_SCAN_POINT_2D);
			if(!scan_ssm.create(SSM_NAME_SCAN_POINT_2D, ssm_prop.id, 5.0, scan_ssm.property.cycle) ){
				::proc_shutoff();
				::fprintf(stderr, "  [\x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m]: Fail to ssm open \"\x1b[4m%s\x1b[0m\"\n", SSM_NAME_SCAN_POINT_2D);
			}
			else {
				if(!scan_ssm.setProperty()) {
					::proc_shutoff();
					::fprintf(stderr, "  [\x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m]: Fail to ssm open \"\x1b[4m%s\x1b[0m\"\n", SSM_NAME_SCAN_POINT_2D);
				}
				else {
					::fprintf(stderr, "  [\x1b[1mOK\x1b[0m]: Open ssm-data \"\x1b[4m%s\x1b[0m\"\n", SSM_NAME_SCAN_POINT_2D);
				}
			}
		} // <--- create sokuiki-data ssm

	} // <--- initialize





	// ---> operation
	if(!::is_proc_shutoff()){
		S2Scan_t *scan_data;
		double scan_htime = 0;
		double recv_htime = 0;
		gnd::inttimer timer_scan;

		gnd::inttimer timer_tmadj;
		double left_tmadj = 0.0;
		bool flg_tmadj = false;
		URGProxy::TimeAdjust prev_tmadj = tmadj;

		gnd::inttimer timer_show(CLOCK_REALTIME, 1.0);
		bool show_st = true;

		int total = 0;
		int scan_psec = 0;
		int npoints = 0;

		{ // ---> scan cycle
			double cycle = 1.0 / ((double)param.revolution / 60);
			timer_scan.begin(CLOCK_REALTIME, cycle / 2.0);
		} // <--- scan cycle

		// ---> time adjust
		if( tmadj_prop.min_poll > 0 ) {
			double tmadj_next;
			struct timeval htime;
			unsigned long dclock;

			if( !::Scip2CMD_TM_GetSyncTime(port, &dclock, &htime) )		::proc_shutoff();
			else URGProxy::timeadjust( &tmadj_prop, &tmadj, &dclock, &htime );

			tmadj_next = URGProxy::timeadjust_polltime( &tmadj );
			timer_tmadj.begin(CLOCK_REALTIME, tmadj_next);
		} // <--- time adjust



		// start scan
		if( !URGProxy::scanning_begin(port, &scan_prop, &buffer) < 0) {
			::proc_shutoff();
		}
		while( !::is_proc_shutoff() ) {
			// wait
			timer_scan.wait();

			// ---> show status
			if( timer_show.clock() && show_st ){
				total += scan_psec;
				::fprintf(stderr, "\x1b[0;0H\x1b[2J");	// display clear
				::fprintf(stderr, "-------------------- \x1b[1m\x1b[36m%s\x1b[39m\x1b[0m --------------------\n", "urg-proxy");
				::fprintf(stderr, "          serial : %s\n", version.serialno );
				::fprintf(stderr, "      total scan : %d\n", total );
				::fprintf(stderr, "    scan / frame : %d\n", scan_psec );
				::fprintf(stderr, "  number of read : %d\n", npoints );
				::fprintf(stderr, "   angular field : %lf to %lf\n", conf_device.angle_range.value[0], conf_device.angle_range.value[1] );
				::fprintf(stderr, "      time-stamp : %lf\n", scan_htime );

				::fprintf(stderr, "     time-adjust : %s\n", flg_tmadj ? "on" : "off" );
				::fprintf(stderr, "                 :        host %.06lf, device %.06lf\n", tmadj.host, tmadj.device );
				::fprintf(stderr, "                 :   prev host %.06lf, device %.06lf\n", prev_tmadj.host, prev_tmadj.device );
				::fprintf(stderr, "                 :transit host %.06lf, device %.06lf\n", tmadj.host - prev_tmadj.host, tmadj.device - prev_tmadj.device );
				::fprintf(stderr, "                 :       drift %.06lf\n", tmadj.drift );
				::fprintf(stderr, "                 :        next %.03lf, poll %d\n", left_tmadj, tmadj.poll );
				::fprintf(stderr, "                 :        diff %.03lf\n", recv_htime - scan_htime );
				::fprintf(stderr, "\n");
				::fprintf(stderr, " Push \x1b[1mEnter\x1b[0m to change CUI Mode\n");
				scan_psec = 0;
			} // <--- show status


			// ---> time adjust.
			if( tmadj_prop.min_poll > 0 && timer_tmadj.clock(&left_tmadj) > 0 ){
				double tmadj_next;
				struct timeval htime;
				unsigned long dclock;

				prev_tmadj = tmadj;

				// stop ms
				if( !::Scip2CMD_StopMS(port, &buffer) ){
					::proc_shutoff();
				}
				else if( !::Scip2CMD_TM_GetSyncTime(port, &dclock, &htime) ) {
					::proc_shutoff();
				}
				else {
					// restart scan
					::S2Sdd_Init( &buffer );
					::S2Sdd_setCallback(&buffer, callback, &recv_time);
					if( !URGProxy::scanning_begin(port, &scan_prop, &buffer) < 0) {
						::proc_shutoff();
					}

					// set clock
					URGProxy::timeadjust( &tmadj_prop, &tmadj, &dclock, &htime );
					tmadj_next = URGProxy::timeadjust_polltime( &tmadj );
					timer_tmadj.begin(CLOCK_REALTIME, tmadj_next);
				}
			} // <--- time adjust



			// ---> sensor reading
			if( S2Sdd_Begin(&buffer, &scan_data) > 0){
				recv_htime = recv_time;
				npoints = URGProxy::scanning_reading(scan_data, &scan_prop, &scan_ssm.data);
				scan_ssm.data.timeStamp( scan_data->time );

				{ // ---> time stamp
					if( tmadj_prop.min_poll > 0 ) {
						URGProxy::timeadjust_device2host(gnd_msec2time(scan_data->time), &tmadj, &scan_htime );
						flg_tmadj = true;
					}
					else {
						scan_htime = recv_htime;
						flg_tmadj = false;
					}
					scan_ssm.write(scan_htime);
				} // <--- time stamp
				// count total scan
				scan_psec++;
				// Don't forget S2Sdd_End to unlock buffer
				S2Sdd_End( &buffer );

			}// <--- sensor reading


			if( ::S2Sdd_IsError(&buffer)){
				break;
			}
		}
	} // <--- operation



	{ // ---> finalize
		::endSSM();
		if(port)	::Scip2_Close(port);
	} // <--- finalize


	return 0;
}
