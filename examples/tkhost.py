import sys 
import tkinter as tk
import moovoo as mv

SIZE=(1200, 800)

class MoovooWindow(tk.Frame):
  def __init__(s, master):
    super().__init__(master)
    s.pack()
    handle = s.master.winfo_id()
    
    s.ctxt = mv.Context()
    s.modelBytes = open("../molecules/2tgt.cif", "rb").read()
    s.model = mv.Model(s.ctxt, s.modelBytes)
    s.view = mv.View(s.ctxt, handle, s.model, SIZE[0], SIZE[1])
    s.data = s.view.render(s.ctxt, s.model)

if __name__ == "__main__":
  try:
    t = tk.Tk() 
    t.resizable(width=False, height=False)
    t.geometry("%dx%d" % SIZE)
    mvw = MoovooWindow(t)
    t.mainloop();
  except KeyboardInterrupt as k:
    print("Keyboard interrupt")

