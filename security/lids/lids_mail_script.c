/* 
 * Mail LIDS security alerts 
 *
 * Philippe Biondi (philippe.biondi@webmotion.com)
 * March 29, 2000
 *  
 *  Sep 1, 2000 Xie Huagang, used configuation file.
 *  	  But It is not compatible to write the pseudo file.
 *
 */


/* 
 * This is a pseudo scripting language 
 * It is in fact directly included in the C source of 
 * klids.c by the cpp.
 *
 */


lids_expect("220");
lids_send(lids_mail_source);
lids_expect("250");
lids_send(lids_mail_from);
lids_expect("250");
lids_send(lids_rcpt_to);
lids_expect("250");
lids_send("data\r\n");
lids_expect("354");
lids_send(lids_mail_subject);
lids_send(LIDS_MESSAGE);
lids_send("\r\n\r\n.\r\n");
lids_expect("250");
lids_send("quit\r\n");
