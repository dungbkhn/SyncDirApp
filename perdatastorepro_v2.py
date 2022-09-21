#!/usr/bin/python

import gi
gi.require_version("Gtk", "3.0")
gi.require_version('AppIndicator3', '0.1')
import os
from gi.repository import Gtk as gtk, AppIndicator3 as appindicator, GLib as glib, Pango

gi.require_version("Notify", "0.7")
from gi.repository import Notify

import subprocess
import signal
import os
import time
import threading

processId = -1
globalX = 0
globalX2 = 0
globalX3 = 0
globalX4 = 0
globalchangepw = 0
globalchangedir = 0
syncdir="/home/dungnt/MySyncDir"
x=0
#win

class ShowRemoteInfoWindow(gtk.Window):
    def __init__(self):
        gtk.Window.__init__(self, title="Remote Information")

        self.set_default_size(400, 250)
        self.set_resizable(False)
        self.icon = self.render_icon(gtk.STOCK_INDEX, 1)
        self.set_icon(self.icon)
        self.box = gtk.Box(orientation=gtk.Orientation.VERTICAL)
        
        self.label = gtk.Label(label="Information of Remote Machine")
        self.label.set_halign(gtk.Align.CENTER)
        self.label.set_valign(gtk.Align.CENTER)
        
        self.con_button = gtk.Button(label="Get Info")
        self.con_button.connect("clicked", self.on_button_clicked)
        
        self.box.pack_start(self.label, False, False, 2)

        self.separator = gtk.Separator()
        self.box.pack_start(self.separator, False, False, 4)
        self.grid = gtk.Grid()
        self.grid.set_hexpand(True)
        self.grid.set_vexpand(True)
        self.box.pack_start(self.grid, True, True, 2)
        self.separator2 = gtk.Separator()
        self.box.pack_start(self.separator2, False, False, 4)
        self.box.pack_start(self.con_button, False, False, 2)
        self.add(self.box)
        self.create_textview()
        
        
    def create_textview(self):
        scrolledwindow = gtk.ScrolledWindow()
        scrolledwindow.set_policy(gtk.PolicyType.NEVER,
                               gtk.PolicyType.AUTOMATIC)
        scrolledwindow.set_hexpand(True)
        scrolledwindow.set_vexpand(True)
        self.grid.attach(scrolledwindow, 0, 0, 1, 1)
        
        self.textview = gtk.TextView()
        self.textview.set_property('editable', False)
        self.textbuffer = self.textview.get_buffer()
        self.textbuffer.set_text(
			"Press Get Info button to get info from remote machine\n"
        )
        scrolledwindow.add(self.textview)

        self.tag_bold = self.textbuffer.create_tag("bold", weight=Pango.Weight.BOLD)
        self.tag_italic = self.textbuffer.create_tag("italic", style=Pango.Style.ITALIC)
        self.tag_underline = self.textbuffer.create_tag(
            "underline", underline=Pango.Underline.SINGLE
        )
       
    def on_button_clicked(self, widget):
        batcmd="bash /home/dungnt/ShellScript/sshsyncapp/changepw.sh 1"
        x = subprocess.check_output(batcmd,shell=True)
        x = str(x)
        print(x)
        x = x.split("'")
        if len(x) > 2:
             x = x[1].split("\\n")
             lst = '\n'.join(x)
             self.textbuffer.set_text(lst)
        
        
    def on_delete_event(event, self, widget):
        global globalX3
        globalX3=0
        print("globalX3 "+str(globalX3))
        self.hide()

        return True
  
class ChangeSyncDirWindow(gtk.Window):
    def __init__(self):
        gtk.Window.__init__(self, title="Change SyncDir")

        self.set_default_size(600, 150)
        self.set_resizable(False)
        self.icon = self.render_icon(gtk.STOCK_INDEX, 1)
        self.set_icon(self.icon)
        self.box = gtk.Box(orientation=gtk.Orientation.VERTICAL)
        global syncdir
        self.entry = gtk.Entry()
        self.entry.set_text(syncdir)
        self.entry.set_halign(gtk.Align.START)
        self.entry.set_valign(gtk.Align.START)
        self.entry.set_editable(False)
        self.entry.set_width_chars(40)
        self.label = gtk.Label(label="Synchronization Directory")
        self.label.set_halign(gtk.Align.CENTER)
        self.label.set_valign(gtk.Align.CENTER)
        
        self.con_button = gtk.Button(label="Change SyncDir")
        self.con_button.connect("clicked", self.on_changedir_button_clicked)
        
        self.pBox = gtk.Box(orientation=gtk.Orientation.HORIZONTAL, spacing=6)
        self.pBox.pack_start(self.label, False, False, 2)
        self.pBox.pack_start(self.entry, False, False, 2)
        self.pBox.pack_start(self.con_button, False, False, 2)
        
        self.box.pack_start(self.pBox, False, False, 2)
       
        
        self.separator1 = gtk.Separator()
        self.box.pack_start(self.separator1, False, False, 4)
        
        self.grid = gtk.Grid()
        self.grid.set_hexpand(True)
        self.grid.set_vexpand(True)
        self.box.pack_start(self.grid, True, True, 2)
        
        self.add(self.box)
        self.create_textview()
        
    def create_textview(self):
        scrolledwindow = gtk.ScrolledWindow()
        scrolledwindow.set_policy(gtk.PolicyType.NEVER,
                               gtk.PolicyType.AUTOMATIC)
        scrolledwindow.set_hexpand(True)
        scrolledwindow.set_vexpand(True)
        self.grid.attach(scrolledwindow, 0, 0, 1, 1)
        
        self.textview = gtk.TextView()
        self.textview.set_property('editable', False)
        self.textbuffer = self.textview.get_buffer()
        self.textbuffer.set_text(
			"Press 'Change SyncDir' button, then type new synchronization directory\n"
        )
        scrolledwindow.add(self.textview)

        self.tag_bold = self.textbuffer.create_tag("bold", weight=Pango.Weight.BOLD)
        self.tag_italic = self.textbuffer.create_tag("italic", style=Pango.Style.ITALIC)
        self.tag_underline = self.textbuffer.create_tag(
            "underline", underline=Pango.Underline.SINGLE
        )
       
    def on_changedir_button_clicked(self, widget):
        global globalchangedir
        if globalchangedir==0:
            globalchangedir=1
            self.con_button.set_label("Apply")
            self.entry.set_editable(True)
            self.textbuffer.set_text("OK, now type new synchronization directory, then press Apply")
	
        elif globalchangedir==1:
            globalchangedir=0
            global syncdir
            mstr=""
            cursyncdir=self.entry.get_text()
            if cursyncdir[-1] == '/':
                cursyncdir=cursyncdir[:-1]
            if syncdir!=cursyncdir:
                batcmd="bash /home/dungnt/ShellScript/sshsyncapp/changepw.sh 2 " + self.entry.get_text()
                x = subprocess.check_output(batcmd,shell=True)
                x = str(x)
                x = x.split("'")
                
                if len(x) > 1:
                   #print(x[1])
                   x = x[1].split("\\n")
                   if len(x) > 1:
                      mstr=mstr+str(x[0]);
                      #print(mstr)
                      if mstr=="Ok...":
                         global processId
                         print("mstr==OK...")
                         syncdir=cursyncdir
                         try:
                             batcmd="kill " + str(processId)
                             print(batcmd)
                             subprocess.check_output(batcmd,shell=True)
                         except:
                             print("no process found")
                         processId = subprocess.Popen(["bash", "/home/dungnt/TestSyncDir/sshsyncdir_v2.sh"]).pid
                         print('newpid '+str(processId))
                         mstr=mstr+"\nRerun successfully"
            else:
                 mstr=mstr+"Synchronization Directory is the same"
                 
            self.con_button.set_label("Change SyncDir")
            self.entry.set_editable(False)
            
            mstr=mstr+"\nReset by press 'Change SyncDir' button, then type new synchronization directory"
            self.textbuffer.set_text(mstr)
            print("globalchangedir" + str(globalchangedir))
			
    def on_delete_event(event, self, widget):
        global globalX4
        globalX4=0
        global globalchangedir
        globalchangedir=0
        print("globalX4 "+str(globalX4))
        self.hide()

        return True
              
class ChangePassWindow(gtk.Window):
    def __init__(self):
        gtk.Window.__init__(self, title="Change Password")

        self.set_default_size(400, 250)
        self.set_resizable(False)
        self.icon = self.render_icon(gtk.STOCK_INDEX, 1)
        self.set_icon(self.icon)
        self.box = gtk.Box(orientation=gtk.Orientation.VERTICAL)
        
        self.entry = gtk.Entry()
        self.entry.set_text("2405:4802:2321:a4e0:872b:27d2:7ce9:ecd7")
        self.entry.set_halign(gtk.Align.START)
        self.entry.set_valign(gtk.Align.START)
        self.entry.set_width_chars(36)
        self.entry.set_editable(False)
        self.label = gtk.Label(label="  IPAddress")
        self.label.set_halign(gtk.Align.CENTER)
        self.label.set_valign(gtk.Align.CENTER)
        
        self.entry2 = gtk.Entry()
        self.entry2.set_text("store")
        self.entry2.set_halign(gtk.Align.START)
        self.entry2.set_valign(gtk.Align.START)
        self.entry2.set_editable(False)
        self.label2 = gtk.Label(label="  Username")
        self.label2.set_halign(gtk.Align.CENTER)
        self.label2.set_valign(gtk.Align.CENTER)
        
        self.pBox = gtk.Box(orientation=gtk.Orientation.HORIZONTAL, spacing=6)
        self.pBox.pack_start(self.label, False, False, 2)
        self.pBox.pack_start(self.entry, False, False, 2)
        

        self.entry3 = gtk.Entry()
        self.entry3.set_text("22")
        self.entry3.set_halign(gtk.Align.START)
        self.entry3.set_valign(gtk.Align.START)
        self.entry3.set_width_chars(6)
        self.entry3.set_editable(False)
        self.label3 = gtk.Label(label="  Port")
        self.label3.set_halign(gtk.Align.CENTER)
        self.label3.set_valign(gtk.Align.CENTER)
        
        self.pBox2 = gtk.Box(orientation=gtk.Orientation.HORIZONTAL, spacing=6)
        self.pBox2.pack_start(self.label2, False, False, 2)
        self.pBox2.pack_start(self.entry2, False, False, 2)
        self.pBox2.pack_start(self.label3, False, False, 2)
        self.pBox2.pack_start(self.entry3, False, False, 2)
        
        self.entry4 = gtk.Entry()
        self.entry4.set_text("xxxxxxxx")
        self.entry4.set_halign(gtk.Align.START)
        self.entry4.set_valign(gtk.Align.START)
        self.entry4.set_visibility(False)
        self.entry4.set_editable(False)
        self.label4 = gtk.Label(label="Current Password")
        self.label4.set_halign(gtk.Align.CENTER)
        self.label4.set_valign(gtk.Align.CENTER)
        
        self.entry5 = gtk.Entry()
        self.entry5.set_text("xxxxxxxx")
        self.entry5.set_halign(gtk.Align.START)
        self.entry5.set_valign(gtk.Align.START)
        self.entry5.set_visibility(False)
        self.entry5.set_editable(False)
        self.label5 = gtk.Label(label="New Password")
        self.label5.set_halign(gtk.Align.CENTER)
        self.label5.set_valign(gtk.Align.CENTER)
        
        self.con_button = gtk.Button(label="Change Password")
        self.con_button.connect("clicked", self.on_apply_button_clicked)
        
        self.pBox3 = gtk.Box(orientation=gtk.Orientation.HORIZONTAL, spacing=6)
        self.pBox3.pack_start(self.label4, False, False, 2)
        self.pBox3.pack_start(self.entry4, False, False, 2)
        self.pBox3.pack_start(self.label5, False, False, 2)
        self.pBox3.pack_start(self.entry5, False, False, 2)
        self.pBox3.pack_start(self.con_button, False, False, 2)
        
        self.separator1 = gtk.Separator()
        self.box.pack_start(self.separator1, False, False, 4)
        self.box.pack_start(self.pBox, False, False, 2)
        self.box.pack_start(self.pBox2, False, False, 2)
        self.separator2 = gtk.Separator()
        self.box.pack_start(self.separator2, False, False, 4)
        self.box.pack_start(self.pBox3, False, False, 2)
        self.separator3 = gtk.Separator()
        self.box.pack_start(self.separator3, False, False, 4)
        self.grid = gtk.Grid()
        self.grid.set_hexpand(True)
        self.grid.set_vexpand(True)
        self.box.pack_start(self.grid, True, True, 2)
        
        self.add(self.box)
        self.create_textview()
        
    def create_textview(self):
        scrolledwindow = gtk.ScrolledWindow()
        scrolledwindow.set_policy(gtk.PolicyType.NEVER,
                               gtk.PolicyType.AUTOMATIC)
        scrolledwindow.set_hexpand(True)
        scrolledwindow.set_vexpand(True)
        self.grid.attach(scrolledwindow, 0, 0, 1, 1)
        
        self.textview = gtk.TextView()
        self.textview.set_property('editable', False)
        self.textbuffer = self.textview.get_buffer()
        self.textbuffer.set_text(
			"Press 'Change Password' button, then type current and new password\n"
        )
        scrolledwindow.add(self.textview)

        self.tag_bold = self.textbuffer.create_tag("bold", weight=Pango.Weight.BOLD)
        self.tag_italic = self.textbuffer.create_tag("italic", style=Pango.Style.ITALIC)
        self.tag_underline = self.textbuffer.create_tag(
            "underline", underline=Pango.Underline.SINGLE
        )
       
    def on_apply_button_clicked(self, widget):
        global globalchangepw
        if globalchangepw==0:
            globalchangepw=1
            self.con_button.set_label("Apply")
            self.entry4.set_editable(True)
            self.entry5.set_editable(True)
            self.textbuffer.set_text("OK, now type current password and new password, then press Apply")

        elif globalchangepw==1:
            globalchangepw=0
            pw = self.entry4.get_text()
            newpw = self.entry5.get_text()
            print(pw)
            print(newpw)
            batcmd="bash /home/dungnt/ShellScript/sshsyncapp/changepw.sh 0 " + str(pw) + " " + str(newpw)
            x = subprocess.check_output(batcmd,shell=True)
            x = str(x)
            print(x)
            x = x.split("\\n")
            x = x[0].split("'")
            if len(x) > 1:
                self.textbuffer.set_text(x[1])
            self.con_button.set_label("Change Password")
            self.entry4.set_editable(False)
            self.entry5.set_editable(False)

			
    def on_delete_event(event, self, widget):
        global globalX2
        globalX2=0
        global globalchangepw
        globalchangepw=0
        print("globalX2 "+str(globalX2))
        self.hide()

        return True
        
class MyWindow(gtk.Window):
    def __init__(self):
        gtk.Window.__init__(self, title="PerDataStoreProjV2")
        gtk.Window.set_default_size(self, 250, 340)
        self.set_resizable(False)
        #self.icon = self.render_icon(gtk.STOCK_INDEX, 1)
        #self.set_icon(self.icon)
        self.box = gtk.Box(orientation=gtk.Orientation.VERTICAL, spacing=6)
        self.add(self.box)    
            
        self.spinner = gtk.Spinner()
        self.box.pack_start(self.spinner, True, True, 2)

        
        self.image = gtk.Image()
        self.box.pack_start(self.image, True, True, 2)
        
        self.label = gtk.Label(label="User")
        self.label.set_halign(gtk.Align.CENTER)
        self.label.set_valign(gtk.Align.CENTER)
        self.box.pack_start(self.label, True, True, 2)
        
        self.subbox = gtk.Box(orientation=gtk.Orientation.HORIZONTAL, spacing=6)
        
        self.button = gtk.Button(label="Details")
        self.button.set_halign(gtk.Align.CENTER)
        self.button.set_valign(gtk.Align.CENTER)
        self.button.connect("clicked", self.on_details_button_clicked)

        
        self.subbox.pack_start(self.button, True, True, 0)
        #self.subbox.pack_start(self.button2, True, True, 0)
        self.box.pack_start(self.subbox, True, True, 2)  
        self.counter = 10
        self.timeout_id = glib.timeout_add(1000, self.on_timeout, None)

        with open("./.temp/mainlog.txt", "r") as file:
            line = file.readline()
            for line in file:
                pass
        try:
            print("lastline of mainlog.txt:"+line)
        except:
            line = '###ok###\n'
        if line == '###ok###\n':
            self.spinner.stop()
            self.label.set_text('Finished!')
            self.image.set_from_file("./images/done.jpg")
        elif line == '###error###\n':
            self.spinner.stop()
            self.label.set_text(' Error due to:\n Client is not ready\n or Remote Machine not reachable\n')
            self.image.set_from_file("./images/error.png")
        elif line == '###error1###\n':
            self.spinner.stop()
            self.label.set_text(' Error due to:\n Execute command at remote side')
            self.image.set_from_file("./images/error.png")
        elif line == '###error2###\n':
            self.spinner.stop()
            self.label.set_text(' Error due to:\n Client process is not running\n')
            self.image.set_from_file("./images/error.png")
        elif line == '###error4###\n':
            self.spinner.stop()
            self.label.set_text(' Error due to:\n Remote machine unreachable!\n Please reinstall the application to fix it!')
            self.image.set_from_file("./images/error.png")
        else:
            self.spinner.start()
            self.label.set_text('Syncing....')
            self.image.set_from_file("./images/anhcungmau.png")

    def on_timeout(self, *args, **kwargs):
        self.counter -= 1
        if self.counter <= 0:
            self.stop_timer("Reached time out")
            return False
        #print("Remaining: " + str(int(self.counter)))
        return True
        
    def stop_timer(self, alabeltext):
        """ Stop the timer. """
        if self.timeout_id:
            #glib.source_remove(self.timeout_id)
            #self.timeout_id = None
            self.counter = 10
            self.timeout_id = glib.timeout_add(1000, self.on_timeout, None)
            
        #global processId
        #batcmd="pgrep -f /home/dungnt/ShellScript/sshsyncapp/sshsyncdir.sh | grep " + str(processId)
        #try:
        #    x = subprocess.check_output(batcmd,shell=True)
        #    print(x)
        #except:
        #    print("no process found")
        print(alabeltext)
        with open("./.temp/mainlog.txt", "r") as file:
            line = file.readline()
            for line in file:
                pass
        try:
            print("lastline of mainlog.txt:"+line)
        except:
            line = '###ok###\n'
        if line == '###ok###\n':
            self.spinner.stop()
            self.label.set_text('Finished!')
            self.image.set_from_file("./images/done.jpg")
        elif line == '###error###\n':
            self.spinner.stop()
            self.label.set_text(' Error due to:\n Client is not ready\n or Remote Machine not reachable\n')
            self.image.set_from_file("./images/error.png")
        elif line == '###error1###\n':
            self.spinner.stop()
            self.label.set_text(' Error due to:\n Execute command at remote side')
            self.image.set_from_file("./images/error.png")
        elif line == '###error2###\n':
            self.spinner.stop()
            self.label.set_text(' Error due to:\n Client process is not running\n')
            self.image.set_from_file("./images/error.png")
        elif line == '###error4###\n':
            self.spinner.stop()
            self.label.set_text(' Error due to:\n Remote machine unreachable!\n Please reinstall the application to fix it!')
            self.image.set_from_file("./images/error.png")
        else:
            self.spinner.start()
            self.label.set_text('Syncing....')
            self.image.set_from_file("./images/anhcungmau.png")
        
    def on_details_button_clicked(self, widget):
    
        os.spawnlp(os.P_NOWAIT, 'gedit', 'gedit', './.temp/mainlog.txt')

    #def on_errors_button_clicked(self, widget):
    #    os.spawnlp(os.P_NOWAIT, 'gedit', 'gedit', './.synctempdir/errors.txt')
        
        
    def on_delete_event(event, self, widget):
        global globalX
        globalX=0
        print(globalX)
        self.hide()
        glib.source_remove(self.timeout_id)
        self.timeout_id = None
		#self.destroy_app()
        return True

def main():
  indicator = appindicator.Indicator.new("customtray", "semi-starred-symbolic", appindicator.IndicatorCategory.APPLICATION_STATUS)
  indicator.set_status(appindicator.IndicatorStatus.ACTIVE)
  indicator.set_menu(menu())
  Notify.init("PerDataStoreProj Application")
  # Create a new notification
  n = Notify.Notification.new("Default Title","Default Body")
  # Update the title / body
  n.update("PerDataStoreProj Application","Started!")
  # Show it
  n.show()
  
  global processId
  global syncdir
  processId = subprocess.Popen(["bash", "sshsyncdir_v2.sh"]).pid
  print('pid '+str(processId))
  

  gtk.main()

  
def menu():
  menu = gtk.Menu()
      
  command_one = gtk.MenuItem(label="Main Menu", use_underline=False)
  command_one.connect('activate', note)
  menu.append(command_one)

  #command_two = gtk.MenuItem(label="Change Password", use_underline=False)
  #command_two.connect('activate', chpw)
  #menu.append(command_two)
  
  command_three = gtk.MenuItem(label="Remote Info", use_underline=False)
  command_three.connect('activate', remoteinfo)
  menu.append(command_three)
  
  #command_four = gtk.MenuItem(label="Change SyncDir", use_underline=False)
  #command_four.connect('activate', chdir)
  #menu.append(command_four)
  
  exittray = gtk.MenuItem(label="Exit", use_underline=True)
  exittray.connect('activate', quit)
  menu.append(exittray)
  
  menu.show_all()
  return menu
  
def note(_):
  global globalX
  if globalX==0:
      win = MyWindow()
      win.connect("delete-event", win.on_delete_event)
      win.show_all()
      globalX=1
      print(globalX)
      
def chdir(_):
  global globalX4
  if globalX4==0:
      win = ChangeSyncDirWindow()
      win.connect("delete-event", win.on_delete_event)
      win.show_all()
      globalX4=1
      print(globalX4)
      
def chpw(_):
  global globalX2
  if globalX2==0:
      win = ChangePassWindow()
      win.connect("delete-event", win.on_delete_event)
      win.show_all()
      globalX2=1
      print(globalX2)
  
def remoteinfo(_):
  global globalX3
  if globalX3==0:
      win = ShowRemoteInfoWindow()
      win.connect("delete-event", win.on_delete_event)
      win.show_all()
      globalX3=1
      print(globalX3)
      
def quit(_):
  
  #global processId
  #batcmd="kill " + str(processId)
  #print(batcmd)
  #try:
  #    subprocess.check_output(batcmd,shell=True)
  #except:
  #	  print("process not found")
	  
  batcmd="bash killchild.sh"
  print(batcmd)
  try:
      subprocess.check_output(batcmd,shell=True)
  except:
	  print("process not found")
	  
  gtk.main_quit()

if __name__ == "__main__":
  main()









	
