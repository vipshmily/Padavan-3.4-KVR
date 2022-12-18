 #!/usr/local/bin/perl

=head POSTING

 From lprng@www.lprng.com Thu Nov 27 10:16:08 2003
 Date: Thu, 27 Nov 2003 17:54:01 +0100 (CET)
 From: Henrik Edlund <henrik@edlund.org>
 To: lprng@lprng.com
 Subject: LPRng: Chooser script using SNMP

 I hereby release the following Chooser script into the public domain on 27
 November 2003.

 The following Chooser script uses SNMP to check printer status, and then
 uses the status information to in a smart way select the _most_ available
 printer. A printer without toner/paper warnings is always chosen over one
 with toner/paper warnings. Within each of those groups; an idle printer is
 chosen first, then a standby printer (after idle because it takes time for
 it to warm up), then a printing printer, and last a warming up printer
 (after printing as it has most likely received a print job and given the
 guess that each print job is of equal size, a printing printer will be
 done before a printer in warm up).

 This script is known to fully work with the Xerox Phaser 4400 and the
 Xerox DocuPrint N32. Your mileage may vary with printers from other
 manufacturers.

 Yours,
   Henrik

 PS. There are "spies" from Xerox on this list; so *wink* *wink* to the
 fellas over at Xerox.

=cut


# Released into the Public Domain by Henrik Edlund <henrik@edlund.org>
# 27 November 2003

# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

use strict;
use warnings;

use Net::SNMP;

my $printers;
my $exit_job_fail = 1;
my $printer;
my $priority;
my $session;
my $error;
my $result;
my $snmp_hr_device_status = '1.3.6.1.2.1.25.3.2.1.5.1';
my $snmp_hr_printer_status = '1.3.6.1.2.1.25.3.5.1.1.1';
my @printers = ([], [], [], [], [], [], [], []);
my $exit_job_success = 0;

# get printers from STDIN
$printers = <STDIN>;
if (not(defined($printers))) {
    # no printers given, try again in a while
    exit($exit_job_fail);
}
chomp($printers);

foreach $printer (split(/,/, $printers)) {
    $priority = -1;
    # talk SNMP to printer
    ($session, $error) = Net::SNMP->session(-hostname => $printer);
    if (defined($session)) {
	# fetch hrDeviceStatus and hrPrinterStatus
	$result = $session->get_request(-varbindlist =>
					[$snmp_hr_device_status,
					 $snmp_hr_printer_status]);
	# check status of printer and assign priority
	if (defined($result)) {
	    if ($result->{$snmp_hr_device_status} == 2) { # running
		if ($result->{$snmp_hr_printer_status} == 3) { # idle
		    $priority = 0;
		}
		elsif ($result->{$snmp_hr_printer_status} == 1) { # standby
		    $priority = 1;
		}
		elsif ($result->{$snmp_hr_printer_status} == 4) { # printing
		    $priority = 2;
		}
		elsif ($result->{$snmp_hr_printer_status} == 5) { # warmup
		    $priority = 3;
		}
	    }
	    elsif ($result->{$snmp_hr_device_status} == 3) { # warning
		if ($result->{$snmp_hr_printer_status} == 3) { # idle
		    $priority = 4;
		}
		elsif ($result->{$snmp_hr_printer_status} == 1) {  # standby
		    $priority = 5;
		}
		elsif ($result->{$snmp_hr_printer_status} == 4) { # printing
		    $priority = 6;
		}
		elsif ($result->{$snmp_hr_printer_status} == 5) { # warmup
		    $priority = 7;
		}
	    }
	    if ($priority != -1) {
		push(@{$printers[$priority]}, $printer);
	    }
	}
	# close the udp transport layer to printer
	$session->close();
    }
}

# of those available with highest priority; pick a random one
foreach $priority (@printers) {
    $printers = scalar(@$priority);
    if ($printers > 0) {
	# success, found available printer
	print "$priority->[rand($printers)]\n";
	exit($exit_job_success);
    }
}

# no printers available right now, try again in a while
exit($exit_job_fail);
