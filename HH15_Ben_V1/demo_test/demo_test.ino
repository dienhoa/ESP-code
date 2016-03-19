//
//String donne = "dfsc\nIMAX 013 C\r\nIINST 000 W\r bhgth \nPAPP 00000 !\r\nHCHC 000042655 \\rsfdwx";
String donne = "azesd\nIMAX 013 C\r\nPAPP 00001 !\r102fdg3\nISOUSC 45 ?\r\nHCHC 000042655 \\rdgdgdsf";
//String donne = "\nIINST 000 W\r";
void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(1200);
}

void loop() {
//  s= checksumverify(donne);
   String s = getData();
  Serial.println(s);

//  Serial.print("Char Array size: ");
//  Serial.println(strlen(donneD));

  delay(1000);
}


char checksumcalc(String data){ // calculate checksum. Sum all characters in the group info, take 6 LSB then + 0x20, (ERDF doc p.18)
  char s=0x00;
  for (int i=0; i < data.length(); i++){
    s += data.charAt(i);
  }
  s = s & 0x3F;
  s = s + 0x20;
  return s;
}

String checksumverify(String data){ // to verify the checksum in each group info in the frame 
  int i=0; // variable to hold position at LF
  int j=0; // variable to hold position at next CR

  while(j<data.length()){
    while((data.charAt(j)!='\r')&&(j<data.length())){
      j++;
    }

   if (j>=data.length()){
      break;
      }
    
    char calcchecksum = checksumcalc(data.substring(i+1,j-2));
    
    if (calcchecksum != data.charAt(j-1)){
      data.setCharAt(j-2,'#'); // delete the part with checksum error
    }
    i=j+1;
    while((data.charAt(i)!='\n')&&(i<data.length())){ // find the new LF
      i++;
    }
   if (i>=data.length()){
      break;
      }
    for (int k=j+1;k<i;k++){data.setCharAt(k,' ');}
//    data.replace(data.substring(j+1,i),""); // delete the unreadable part between CR to new LF
    j=i+1;
  }

  return data;
}

String getData(){
  String data = "{\"value\":\"";
  String str = donne;
  str.replace("\\r","\\\r");
  int LF_pos = str.indexOf('\n'); 
  int CR_pos = str.lastIndexOf('\r');
  
  str = str.substring(LF_pos,CR_pos+1);//remove the unreadale char at the beginning and the end of the frame
  str= checksumverify(str);
  str.replace("\n", " ");
  str.replace("\r", " ");
  str.replace("\\r", "\\ ");
  data += str;
  data += "\"}";;
  return data;
}

