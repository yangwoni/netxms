/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2010 Victor Kirhenshtein
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.ui.eclipse.reporter.dialogs;

import java.io.File;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;
import org.netxms.ui.eclipse.filemanager.widgets.LocalFileSelector;
import org.netxms.ui.eclipse.tools.WidgetHelper;

/**
 * Object creation dialog
 *
 */
public class CreateReportDialog extends Dialog
{
	private String objectName;
	private Text textName;
	private LocalFileSelector fileSelector;

	/**
	 * Constructor
	 * 
	 * @param parentShell Parent shell
	 * @param objectClassName Object class - this string will be added to dialog's title
	 */
	public CreateReportDialog(Shell parentShell)
	{
		super(parentShell);
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.window.Window#configureShell(org.eclipse.swt.widgets.Shell)
	 */
	@Override
	protected void configureShell(Shell newShell)
	{
		super.configureShell(newShell);
		newShell.setText("Create Report");
	}
	
	/* (non-Javadoc)
	 * @see org.eclipse.jface.dialogs.Dialog#createDialogArea(org.eclipse.swt.widgets.Composite)
	 */
	@Override
	protected Control createDialogArea(Composite parent)
	{
		Composite dialogArea = (Composite)super.createDialogArea(parent);

		GridLayout layout = new GridLayout();
      layout.marginWidth = WidgetHelper.DIALOG_WIDTH_MARGIN;
      layout.marginHeight = WidgetHelper.DIALOG_HEIGHT_MARGIN;
      dialogArea.setLayout(layout);
		
      textName = WidgetHelper.createLabeledText(dialogArea, SWT.SINGLE | SWT.BORDER, SWT.DEFAULT,
      		"Object name", "", WidgetHelper.DEFAULT_LAYOUT_DATA);
      textName.getShell().setMinimumSize(300, 0);
      textName.setTextLimit(63);
      textName.setFocus();
      
      fileSelector = new LocalFileSelector(dialogArea, SWT.NONE);
      fileSelector.setLabel("Report definition file");
      GridData gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      fileSelector.setLayoutData(gd);
      
		return dialogArea;
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.dialogs.Dialog#okPressed()
	 */
	@Override
	protected void okPressed()
	{
		objectName = textName.getText().trim();
		if (objectName.isEmpty())
		{
			MessageDialog.openWarning(getShell(), "Warning", "Please enter valid object name");
			return;
		}
		super.okPressed();
	}

	/**
	 * @return the objectName
	 */
	public String getObjectName()
	{
		return objectName;
	}
	
	/**
	 * Get selected report definition file
	 * 
	 * @return report definition file or null
	 */
	public File getDefinitionFile()
	{
		return fileSelector.getFile();
	}
}
