import moovoo as mv

size = (1200, 1024)

ctxt = mv.Context()


modelBytes = open("../molecules/2tgt.cif", "rb").read()
model = mv.Model(ctxt, modelBytes)
view = mv.View(ctxt, "Window", model, size)

##data = view.render(ctxt, model)

ctxt.mainloop()

del view
del model
del ctxt



